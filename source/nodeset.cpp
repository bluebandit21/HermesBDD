/***************************************************************************
 *            nodeset.cpp
 *
 *  Copyright  2021  Luigi Capogrosso and Luca Geretti
 *
 ****************************************************************************/

/*
 * MIT License
 *
 * Copyright (c) 2021 Luigi Capogrosso and Luca Geretti
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



#include <cassert>

#include "hash.hpp"
#include "nodeset.hpp"

/*!
 * @param node
 * @return
 */
static inline size_t hash(const Node& node)
{
    return hash128(&node, sizeof(Node));
}

/*!
 * @param src
 * @param dest
 */
static void copy_node(const Node& src, Node& dest)
{
    dest.root = src.root;
    dest.branch_true = src.branch_true;
    dest.branch_false = src.branch_false;
    dest.size = src.size;
}

void NodeSet::init(size_t mem_usage)
{
    elements = mem_usage / sizeof(NodeSlot);
    assert(elements <std::numeric_limits<int32_t>::max());
    // Much faster than running constructors.
    table = (NodeSlot *) calloc(elements, sizeof(NodeSlot));

    assert(table != nullptr);
    table[0].exists = true;
}

/*!
 * A lock guard around nodes
 */
struct LockProtector
{
public:
    LockProtector(NodeSlot& slot) : _slot(slot)
    {
        while (_slot.locked.test_and_set(std::memory_order_acquire));
    }

    ~LockProtector()
    {
        _slot.locked.clear(std::memory_order_release);
    }

private:
    NodeSlot& _slot;
};

uint32_t NodeSet::lookup_create(Node node)
{
    uint32_t hashed = hash(node);

    for (uint32_t offset = 0; offset < elements; offset++)
    {
        uint32_t index = (hashed + offset) % elements;

        NodeSlot& current = table[index];
        LockProtector lock(current);

        if (!current.exists)
        {
            copy_node(node, current.node);

            // Maintain data structure.
            count.fetch_add(1, std::memory_order_relaxed);
            current.exists = true;

            return index;
        }

        if (node.root == current.node.root               &&
            node.branch_true == current.node.branch_true &&
            node.branch_false == current.node.branch_false)
        {
            // TODO: increase reference count.
            return index;
        }
    }

    return 0;
}

