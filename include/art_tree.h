
#ifndef ART_TEST_ART_TREE_H
#define ART_TEST_ART_TREE_H

#include <iostream>
#include <stdlib.h>    // malloc, free
#include <string.h>    // memset, memcpy
#include <stdint.h>    // integer types
#include <emmintrin.h> // x86 SSE intrinsics
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>  // gettime
#include <algorithm>   // std::random_shuffle
#include <map>
#include <vector>
#include <utility>

namespace alert {

// Contains a version of ART that supports lower & upper bound lookups.

// Uses ART as a non-clustered primary index that stores <key, offset> pairs.
// At lookup time, we retrieve a key's offset from ART and lookup its value in the data array.
    template<class KeyType>
    class ArtTree {
    public:

        ArtTree() {
            allocated_byte_count = 0;
        }

        ~ArtTree() { destructTree(tree_); }

        ArtTree(const ArtTree &) = delete;
        ArtTree &operator=(ArtTree & tree) = delete;

        ArtTree(ArtTree && t): tree_(t.tree_) {
            t.tree_ = nullptr;
        }

        ArtTree &operator=(ArtTree && t) {
            tree_ = t.tree_;
            t.tree_ = nullptr;
            return *this;
        }


        void Remove(KeyType key) {
            uint8_t reverse_key[KEY_SIZE];
            swapBytes(key, reverse_key);
            erase(reverse_key);
        }

        void* LowerBound(KeyType key) const {
            uint8_t reverse_key[KEY_SIZE];
            swapBytes(key, reverse_key);
            // Lower bound lookup.
            Iterator it;
            const bool found = bound(tree_, reverse_key,  it);
            if (found)
                return reinterpret_cast<void*>(it.value->value);
            return nullptr;
        }

        uint64_t SumUp(KeyType lookup_key) {
            uint8_t reverse_key[KEY_SIZE];
            swapBytes(lookup_key, reverse_key);
            // Lower bound lookup.
            Iterator it;
            const bool found = bound(tree_, reverse_key,  it);
            if (!found) return 0;
            uint64_t sum = 0;
            KeyType key;
            while (true) {
                LeafNode *leaf = it.value;
                getOriginKey(key, leaf->key);
                if (key != lookup_key) break;
                sum += leaf->value;
                if (!iteratorNext(it)) break;
            }
            return sum;
        }


        void Range(KeyType start_key, KeyType end_key, std::vector<std::pair<KeyType, uint64_t>> &data) {
            uint8_t reverse_key[KEY_SIZE];
            swapBytes(start_key, reverse_key);
            // Lower bound lookup.
            Iterator it;
            const bool found = bound(tree_, reverse_key,  it);
            if (!found) return;
            KeyType key;
            while (true) {
                LeafNode *leaf = it.value;
                getOriginKey(key, leaf->key);
                if (key >= end_key) break;
                data.emplace_back(key, leaf->value);
                if (!iteratorNext(it)) break;
            }
        }

        void* Lookup(KeyType lookup_key)  {
            uint8_t reverse_key[KEY_SIZE];
            swapBytes(lookup_key, reverse_key);
            LeafNode* leaf = lookup(tree_,reverse_key, 0);
            if (leaf) return reinterpret_cast<void*>(leaf->value);
            return nullptr;
        }

        void Insert(KeyType key, uintptr_t value) {
            uint8_t reverse_key[KEY_SIZE];
            swapBytes(key, reverse_key);
            insert(tree_, &tree_, reverse_key, 0, value);
        }


        std::size_t size() const {
            return sizeof(*this) + allocated_byte_count;
        }

        void print_node_msg() {
            std::cout << "node4: " << node4_num << " node16: " << node16_num << " node48: " << node48_num <<  " node256: " << node256_num << std::endl;
        }


    private:
        static uint64_t allocated_byte_count; // track bytes allocated

        static const size_t KEY_SIZE = sizeof(KeyType);

        // Constants for the node types
        static const int8_t NodeType4 = 0;
        static const int8_t NodeType16 = 1;
        static const int8_t NodeType48 = 2;
        static const int8_t NodeType256 = 3;

        static uint64_t node4_num;
        static uint64_t node16_num;
        static uint64_t node48_num;
        static uint64_t node256_num;


        // Shared header of all inner nodes
        struct Node {
            // number of non-null children
            uint16_t count;
            // node type
            int8_t type;

            Node(int8_t type) : count(0), type(type) {}

            ~Node() {
                switch (type) {
                    case NodeType4: {
                        allocated_byte_count -= sizeof(Node4);
                        break;
                    }
                    case NodeType16: {
                        allocated_byte_count -= sizeof(Node16);
                        break;
                    }
                    case NodeType48: {
                        allocated_byte_count -= sizeof(Node48);
                        break;
                    }
                    case NodeType256: {
                        allocated_byte_count -= sizeof(Node256);
                        break;
                    }
                }
            }
        };


        // Represents a leaf.
        struct LeafNode {
            uint8_t key[KEY_SIZE];
            uintptr_t value;
            LeafNode() {
                allocated_byte_count += sizeof(*this);
            }
            ~LeafNode() {
                allocated_byte_count -= sizeof(*this);
            }
        };

        // Node with up to 4 children
        struct Node4 : Node {
            uint8_t key[4];
            Node *child[4];

            Node4() : Node(NodeType4) {
                memset(key, 0, sizeof(key));
                memset(child, 0, sizeof(child));
                allocated_byte_count += sizeof(*this);
//                node4_num++;
            }
        };

        // Node with up to 16 children
        struct Node16 : Node {
            uint8_t key[16];
            Node *child[16];

            Node16() : Node(NodeType16) {
                memset(key, 0, sizeof(key));
                memset(child, 0, sizeof(child));
                allocated_byte_count += sizeof(*this);
//                node16_num++;
            }
        };

        static const uint8_t emptyMarker = 48;

        // Node with up to 48 children
        struct Node48 : Node {
            uint8_t childIndex[256];
            Node *child[48];

            Node48() : Node(NodeType48) {
                memset(childIndex, emptyMarker, sizeof(childIndex));
                memset(child, 0, sizeof(child));
                allocated_byte_count += sizeof(*this);
//                node48_num++;
            }
        };

        // Node with up to 256 children
        struct Node256 : Node {
            Node *child[256];

            Node256() : Node(NodeType256) {
                memset(child, 0, sizeof(child));
                allocated_byte_count += sizeof(*this);
//                node256_num++;
            }
        };

        inline Node *makeLeaf(uint8_t key[], uintptr_t value) {
            // Create a pseudo-leaf
            LeafNode *leaf = new LeafNode;
            memcpy(leaf->key, key, KEY_SIZE);
            leaf->value = value;
            return reinterpret_cast<Node *>((((uintptr_t)leaf) << 1) | 1);
        }

        inline bool isLeaf(Node *node) const{
            // Is the node a leaf?
            return reinterpret_cast<uintptr_t>(node) & 1;
        }

        inline LeafNode* getLeafValue(Node *node) const{
            // The the value stored in the pseudo-leaf
            return reinterpret_cast<LeafNode*>(((uintptr_t)node) >> 1) ;
        }



        struct IteratorEntry {
            Node *node;
            int pos;
        };

        struct Iterator {
            /// The current value, valid if depth>0
            LeafNode* value;
            /// The current depth
            uint32_t depth;
            /// Stack, actually the size is determined at runtime
            IteratorEntry stack[9];
        };

        static inline unsigned ctz(uint16_t x) {
            // Count trailing zeros, only defined for x>0
#ifdef __GNUC__
            return __builtin_ctz(x);
#else
            // Adapted from Hacker's Delight
         unsigned n=1;
         if ((x&0xFF)==0) {n+=8; x=x>>8;}
         if ((x&0x0F)==0) {n+=4; x=x>>4;}
         if ((x&0x03)==0) {n+=2; x=x>>2;}
         return n-(x&1);
#endif
        }

        // This address is used to communicate that search failed
        Node *nullNode = nullptr;

        Node **findChild(Node *n, uint8_t keyByte) {
            // Find the next child for the keyByte
            switch (n->type) {
                case NodeType4: {
                    Node4 *node = static_cast<Node4 *>(n);
                    for (unsigned i = 0; i < node->count; i++)
                        if (node->key[i] == keyByte)
                            return &node->child[i];
                    return &nullNode;
                }
                case NodeType16: {
                    Node16 *node = static_cast<Node16 *>(n);
                    __m128i cmp = _mm_cmpeq_epi8(_mm_set1_epi8(keyByte),
                                                 _mm_loadu_si128(reinterpret_cast<__m128i *>(node->key)));
                    unsigned bitfield = _mm_movemask_epi8(cmp) & ((1 << node->count) - 1);
                    if (bitfield)
                        return &node->child[ctz(bitfield)];
                    else
                        return &nullNode;
                }
                case NodeType48: {
                    Node48 *node = static_cast<Node48 *>(n);
                    if (node->childIndex[keyByte] != emptyMarker)
                        return &node->child[node->childIndex[keyByte]];
                    else
                        return &nullNode;
                }
                case NodeType256: {
                    Node256 *node = static_cast<Node256 *>(n);
                    return &(node->child[keyByte]);
                }
            }
            throw; // Unreachable
        }

        bool iteratorNext(Iterator &iter) const{
            // Skip leaf
            if ((iter.depth) && (isLeaf(iter.stack[iter.depth - 1].node)))
                iter.depth--;

            // Look for next leaf
            while (iter.depth) {
                Node *node = iter.stack[iter.depth - 1].node;

                // Leaf found
                if (isLeaf(node)) {
                    iter.value = getLeafValue(node);
                    return true;
                }

                // Find next node
                Node *next = nullptr;
                switch (node->type) {
                    case NodeType4: {
                        Node4 *n = static_cast<Node4 *>(node);
                        if (iter.stack[iter.depth - 1].pos < node->count)
                            next = n->child[iter.stack[iter.depth - 1].pos++];
                        break;
                    }
                    case NodeType16: {
                        Node16 *n = static_cast<Node16 *>(node);
                        if (iter.stack[iter.depth - 1].pos < node->count)
                            next = n->child[iter.stack[iter.depth - 1].pos++];
                        break;
                    }
                    case NodeType48: {
                        Node48 *n = static_cast<Node48 *>(node);
                        unsigned depth = iter.depth - 1;
                        for (; iter.stack[depth].pos < 256; iter.stack[depth].pos++)
                            if (n->childIndex[iter.stack[depth].pos] != emptyMarker) {
                                next = n->child[n->childIndex[iter.stack[depth].pos++]];
                                break;
                            }
                        break;
                    }
                    case NodeType256: {
                        Node256 *n = static_cast<Node256 *>(node);
                        unsigned depth = iter.depth - 1;
                        for (; iter.stack[depth].pos < 256; iter.stack[depth].pos++)
                            if (n->child[iter.stack[depth].pos]) {
                                next = n->child[iter.stack[depth].pos++];
                                break;
                            }
                        break;
                    }
                }

                if (next) {
                    iter.stack[iter.depth].pos = 0;
                    iter.stack[iter.depth].node = next;
                    iter.depth++;
                } else
                    iter.depth--;
            }

            return false;
        }

        static void swapBytes(uint64_t org_key, uint8_t key[]) {
            reinterpret_cast<uint64_t *>(key)[0] = __builtin_bswap64(org_key);
        }

        static void swapBytes(uint32_t org_key, uint8_t key[]) {
            reinterpret_cast<uint32_t *>(key)[0] = __builtin_bswap32(org_key);
        }

        static void getOriginKey(uint32_t &key, uint8_t* reverse_key) {
            reinterpret_cast<uint32_t *>(&key)[0] = __builtin_bswap64(*(reinterpret_cast<uint32_t*>(reverse_key)));
        }

        static void getOriginKey(uint64_t &key, uint8_t* reverse_key) {
            reinterpret_cast<uint64_t *>(&key)[0] = __builtin_bswap64(*(reinterpret_cast<uint64_t*>(reverse_key)));
        }

        inline uint8_t* loadKey(LeafNode* leaf) const{
            // Store the key of the tuple into the key vector
            // Implementation is database specific
            return leaf->key;
        }

        inline bool less_than(LeafNode *leaf, uint8_t* key) {
            uint8_t *leaf_key = loadKey(leaf);
            return memcmp(leaf_key, key, KEY_SIZE) < 0;
        }

        bool leafMatches(Node *leaf,
                         uint8_t key[],
                         unsigned depth) {
            // Check if the key of the leaf is equal to the searched key
            if (depth != KEY_SIZE) {
                uint8_t *leafKey = loadKey(getLeafValue(leaf));
                for (unsigned i = depth; i < KEY_SIZE; i++)
                    if (leafKey[i] != key[i])
                        return false;
            }
            return true;
        }

        bool bound(Node *n,
                   uint8_t key[],
                   Iterator &iterator) const {
            iterator.depth = 0;

            if (!n)
                return false;

            unsigned depth = 0;
            while (true) {
                iterator.stack[iterator.depth].node = n;
                int &pos = iterator.stack[iterator.depth].pos;
                iterator.depth++;

                if (isLeaf(n)) {
                    iterator.value = getLeafValue(n);
                    if (depth == KEY_SIZE) {
                        // Equal
                        return true;
                    }

                    uint8_t *leafKey = loadKey(getLeafValue(n));
                    for (unsigned i = depth; i < KEY_SIZE; i++)
                        if (leafKey[i] != key[i]) {
                            if (leafKey[i] < key[i]) {
                                // Less
                                iterator.depth--;
                                return iteratorNext(iterator);
                            }
                            // Greater
                            return true;
                        }

                    return true;

                }

                uint8_t keyByte = key[depth];

                Node *next = nullptr;
                switch (n->type) {
                    case NodeType4: {
                        Node4 *node = static_cast<Node4 *>(n);
                        for (pos = 0; pos < node->count; pos++)
                            if (node->key[pos] == keyByte) {
                                next = node->child[pos];
                                break;
                            } else if (node->key[pos] > keyByte)
                                break;
                        break;
                    }
                    case NodeType16: {
                        Node16 *node = static_cast<Node16 *>(n);

#ifdef __x86_64__
                        __m128i cmp;

                        // Compare the key to all 16 stored keys
                        // less equal
                        __m128i a = _mm_set1_epi8(keyByte);

                        cmp = _mm_cmpeq_epi8(_mm_min_epu8(a, _mm_loadu_si128((__m128i*)node->key)), a);

                        unsigned bitfield = _mm_movemask_epi8(cmp) & ((1 << node->count) - 1);

                        // Check if less than any
                        if (bitfield) {
                            pos = __builtin_ctz(bitfield);
                            if (node->key[pos] == keyByte) {
                                next = node->child[pos];
                            }
                        } else pos = node->count;
#else
                        for (pos = 0; pos < node->count; pos++)
                            if (node->key[pos] == keyByte) {
                                next = node->child[pos];
                                break;
                            } else if (node->key[pos] > keyByte)
                                break;
#endif
                        break;
                    }
                    case NodeType48: {
                        Node48 *node = static_cast<Node48 *>(n);
                        pos = keyByte;
                        if (node->childIndex[keyByte] != emptyMarker) {
                            next = node->child[node->childIndex[keyByte]];
                            break;
                        }
                        break;
                    }
                    case NodeType256: {
                        Node256 *node = static_cast<Node256 *>(n);
                        pos = keyByte;
                        next = node->child[keyByte];
                        break;
                    }
                }

                if (!next)
                    return iteratorNext(iterator);

                pos++;
                n = next;
                depth++;
            }
        }

        LeafNode* lookup(Node* node,
                     uint8_t key[],
                     unsigned depth) {

            while (node != nullptr) {
                if (isLeaf(node)) {
                    if (leafMatches(node, key, depth))
                        return getLeafValue(node);
                    else return nullptr;
                }

                node = *findChild(node, key[depth]);
                depth++;
            }

            return nullptr;
        }

        void insertNode4(Node4 *node, Node **nodeRef, uint8_t keyByte, Node *child) {
            // Insert leaf into inner node
            if (node->count < 4) {
                // Insert element
                unsigned pos;
                for (pos = 0; (pos < node->count) && (node->key[pos] < keyByte); pos++);
                memmove(node->key + pos + 1, node->key + pos, node->count - pos);
                memmove(node->child + pos + 1,
                        node->child + pos,
                        (node->count - pos) * sizeof(uintptr_t));
                node->key[pos] = keyByte;
                node->child[pos] = child;
                node->count++;
            } else {
                // Grow to Node16
                Node16 *newNode = new Node16();
                *nodeRef = newNode;
                newNode->count = node->count;
                memcpy(newNode->key, node->key, node->count * sizeof(uint8_t));
                memcpy(newNode->child, node->child, node->count * sizeof(uintptr_t));
                delete node;
                return insertNode16(newNode, nodeRef, keyByte, child);
            }
        }

        void insertNode16(Node16 *node,
                          Node **nodeRef,
                          uint8_t keyByte,
                          Node *child) {
            // Insert leaf into inner node
            if (node->count < 16) {

                // support x86-64 architectures
#ifdef __x86_64__
                unsigned mask = (1 << node->count) - 1;
                __m128i cmp, a;
                a = _mm_set1_epi8(keyByte);

                // Compare the key to all 16 stored keys
                cmp = _mm_cmpeq_epi8(_mm_min_epu8(a, _mm_loadu_si128((__m128i*)node->key)), a);

                // Use a mask to ignore children that don't exist
                unsigned bitfield = _mm_movemask_epi8(cmp) & mask;

                // Check if less than any
                unsigned pos = 0;
                if (bitfield) {
                    pos = __builtin_ctz(bitfield);
                    memmove(node->key + pos + 1, node->key + pos, node->count - pos);
                    memmove(node->child + pos + 1,
                            node->child + pos,
                            (node->count - pos) * sizeof(uintptr_t));
                } else
                    pos = node->count;
#else

                // Insert element
                unsigned pos = 0;
                while ((pos < node->count) && (node->key[pos] < keyByte))
                    pos++;
                memmove(node->key + pos + 1, node->key + pos, node->count - pos);
                memmove(node->child + pos + 1,
                        node->child + pos,
                        (node->count - pos) * sizeof(uintptr_t));

#endif

                node->key[pos] = keyByte;
                node->child[pos] = child;
                node->count++;
            } else {
                // Grow to Node48
                Node48 *newNode = new Node48();
                *nodeRef = newNode;
                memcpy(newNode->child, node->child, node->count * sizeof(uintptr_t));
                for (unsigned i = 0; i < node->count; i++)
                    newNode->childIndex[node->key[i]] = i;
                newNode->count = node->count;
                delete node;
                return insertNode48(newNode, nodeRef, keyByte, child);
            }
        }

        void insertNode48(Node48 *node,
                          Node **nodeRef,
                          uint8_t keyByte,
                          Node *child) {
            // Insert leaf into inner node
            if (node->count < 48) {
                // Insert element
                unsigned pos = node->count;
                if (node->child[pos])
                    for (pos = 0; node->child[pos] != nullptr; pos++);
                node->child[pos] = child;
                node->childIndex[keyByte] = pos;
                node->count++;
            } else {
                // Grow to Node256
                Node256 *newNode = new Node256();
                for (unsigned i = 0; i < 256; i++)
                        if (node->childIndex[i] != emptyMarker)
                        newNode->child[i] = node->child[node->childIndex[i]];
                newNode->count = node->count;
                *nodeRef = newNode;
                delete node;
                return insertNode256(newNode, nodeRef, keyByte, child);
            }
        }

        void insertNode256(Node256 *node,
                           Node **nodeRef,
                           uint8_t keyByte,
                           Node *child) {
            // Insert leaf into inner node
            node->count++;
            node->child[keyByte] = child;
        }

        void insert(Node *node,
                    Node **nodeRef,
                    uint8_t key[],
                    unsigned depth,
                    uintptr_t value) {
            // Insert the leaf value into the tree

            if (node == nullptr) {
                *nodeRef = makeLeaf(key, value);
                return;
            }

            if (isLeaf(node)) {
                // Replace leaf with Node4 and store both leaves in it
                if (leafMatches(node, key, depth)) {
                    getLeafValue(node)->value = value;
                    return;
                }

                LeafNode* existingLeaf = getLeafValue(node);
                uint8_t* existingKey = loadKey(existingLeaf);

                Node4 *topNode = new Node4();
                *nodeRef = topNode;

                for (; depth < KEY_SIZE && existingKey[depth] == key[depth]; ++depth) {
                    Node4 *midNode = new Node4();
                    insertNode4(topNode, nodeRef, key[depth], midNode);
                    topNode = midNode;
                }

                insertNode4(topNode, nodeRef, existingKey[depth], node);
                insertNode4(topNode,
                                nodeRef,
                                key[depth],
                                makeLeaf(key, value));

                return;
            }

            // Recurse
            Node **child = findChild(node, key[depth]);
            if (*child) {
                insert(*child, child, key, depth + 1, value);
                return;
            }

            // Insert leaf into inner node
            Node *newNode = reinterpret_cast<Node *>(makeLeaf(key, value));
            switch (node->type) {
                case NodeType4:
                    insertNode4(static_cast<Node4 *>(node),
                                nodeRef,
                                key[depth],
                                newNode);
                    break;
                case NodeType16:
                    insertNode16(static_cast<Node16 *>(node),
                                 nodeRef,
                                 key[depth],
                                 newNode);
                    break;
                case NodeType48:
                    insertNode48(static_cast<Node48 *>(node),
                                 nodeRef,
                                 key[depth],
                                 newNode);
                    break;
                case NodeType256:
                    insertNode256(static_cast<Node256 *>(node),
                                  nodeRef,
                                  key[depth],
                                  newNode);
                    break;
            }
        }

        struct Context {
            Node *node;
            Node **nodeRef;
        };


        void erase(uint8_t key[]) {
            // Delete a leaf from a tree
            if (!tree_)
                return;

            // Handle hitting a single leaf node
            if (isLeaf(tree_)) {
                // Make sure we have the right leaf
                if (leafMatches(tree_, key, 0)) {
                    delete getLeafValue(tree_);
                    tree_ = nullptr;
                }
                return;
            }

            unsigned sp = 0;
            Context stack[KEY_SIZE];

            unsigned depth = 0;
            Node *node = tree_, **nodeRef = &tree_;

            while (node) {
                stack[sp] = {node, nodeRef};
                Node **child = findChild(node, key[depth]);
                if (isLeaf(*child)
                    && leafMatches(*child, key, depth)) {
                    delete getLeafValue(*child);
                    // Leaf found, delete it in inner node
                    switch (node->type) {
                        case NodeType4:
                            eraseNode4(stack, sp, child);
                            break;
                        case NodeType16:
                            eraseNode16(static_cast<Node16 *>(node),
                                        nodeRef,
                                        child);
                            break;
                        case NodeType48:
                            eraseNode48(static_cast<Node48 *>(node),
                                        nodeRef,
                                        key[depth]);
                            break;
                        case NodeType256:
                            eraseNode256(static_cast<Node256 *>(node),
                                         nodeRef,
                                         key[depth]);
                            break;
                    }
                    break;
                } else {
                    //Recurse
                    node = *child;
                    nodeRef = child;
                    depth += 1;
                    sp += 1;
                }
            }
        }


        void eraseNode4(Context *stack, unsigned sp, Node **leafPlace) {
            Node4 *node = reinterpret_cast<Node4*>(stack[sp].node);
            Node **nodeRef = stack[sp].nodeRef;
            // Delete leaf from inner node
            unsigned pos = leafPlace - node->child;
            memmove(node->key + pos, node->key + pos + 1, node->count - pos - 1);
            memmove(node->child + pos,
                    node->child + pos + 1,
                    (node->count - pos - 1) * sizeof(uintptr_t));
            node->count--;

//            if (node->count == 1) {
//                // Get rid of one-way node
//                Node *child = node->child[0];
//                if (isLeaf(child)) {
//                    while (node->count == 1) {
//                        delete node;
//                        if (sp == 0)
//                            break;
//                        sp -= 1;
//                        node = reinterpret_cast<Node4*>(stack[sp].node);
//                        nodeRef = stack[sp].nodeRef;
//                    }
//                    *nodeRef = child;
//                }
//            }

        }

        void eraseNode16(Node16 *node, Node **nodeRef, Node **leafPlace) {
            // Delete leaf from inner node
            unsigned pos = leafPlace - node->child;
            memmove(node->key + pos, node->key + pos + 1, node->count - pos - 1);
            memmove(node->child + pos,
                    node->child + pos + 1,
                    (node->count - pos - 1) * sizeof(uintptr_t));
            node->count--;

            if (node->count == 3) {
                // Shrink to Node4
                Node4 *newNode = new Node4();
                newNode->count = node->count;
                for (unsigned i = 0; i < node->count; i++)
                    newNode->key[i] = node->key[i];
                memcpy(newNode->child, node->child, sizeof(uintptr_t) * node->count);
                *nodeRef = newNode;
                delete node;
            }
        }

        void eraseNode48(Node48 *node, Node **nodeRef, uint8_t keyByte) {
            // Delete leaf from inner node
            node->child[node->childIndex[keyByte]] = nullptr;
            node->childIndex[keyByte] = emptyMarker;
            node->count--;

            if (node->count == 12) {
                // Shrink to Node16
                Node16 *newNode = new Node16();
                *nodeRef = newNode;
                for (unsigned b = 0; b < 256; b++) {
                    if (node->childIndex[b] != emptyMarker) {
                        newNode->key[newNode->count] = b;
                        newNode->child[newNode->count] = node->child[node->childIndex[b]];
                        newNode->count++;
                    }
                }
                delete node;
            }
        }

        void eraseNode256(Node256 *node, Node **nodeRef, uint8_t keyByte) {
            // Delete leaf from inner node
            node->child[keyByte] = nullptr;
            node->count--;

            if (node->count == 37) {
                // Shrink to Node48
                Node48 *newNode = new Node48();
                *nodeRef = newNode;
                for (unsigned b = 0; b < 256; b++) {
                    if (node->child[b]) {
                        newNode->childIndex[b] = newNode->count;
                        newNode->child[newNode->count] = node->child[b];
                        newNode->count++;
                    }
                }
                delete node;
            }
        }

        void destructTree(Node *node) {
            if (!node) return;

            if (isLeaf(node)) {
                delete getLeafValue(node);
                return ;
            }

            switch (node->type) {
                case NodeType4: {
                    auto n4 = static_cast<Node4 *>(node);
                    for (auto i = 0; i < node->count; i++) {
                        if (!isLeaf(n4->child[i])) {
                            destructTree(n4->child[i]);
                        }
                    }
                    delete n4;
                    break;
                }
                case NodeType16: {
                    auto n16 = static_cast<Node16 *>(node);
                    for (auto i = 0; i < node->count; i++) {
                        if (!isLeaf(n16->child[i])) {
                            destructTree(n16->child[i]);
                        }
                    }
                    delete n16;
                    break;
                }
                case NodeType48: {
                    auto n48 = static_cast<Node48 *>(node);
                    for (auto i = 0; i < 256; i++) {
                        if (n48->childIndex[i] != emptyMarker && !isLeaf(n48->child[n48->childIndex[i]])) {
                            destructTree(n48->child[n48->childIndex[i]]);
                        }
                    }
                    delete n48;
                    break;
                }
                case NodeType256: {
                    auto n256 = static_cast<Node256 *>(node);
                    for (auto i = 0; i < 256; i++) {
                        if (n256->child[i] != nullptr && !isLeaf(n256->child[i])) {
                            destructTree(n256->child[i]);
                        }
                    }
                    delete n256;
                    break;
                }
            }
        }

        Node *tree_ = nullptr;
    };

}

#endif //ART_TREE_H