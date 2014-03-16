/********************************************************************************
* ReactPhysics3D physics library, http://code.google.com/p/reactphysics3d/      *
* Copyright (c) 2010-2014 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

#ifndef REACTPHYSICS3D_DYNAMIC_AABB_TREE_H
#define REACTPHYSICS3D_DYNAMIC_AABB_TREE_H

// Libraries
#include "../../configuration.h"
#include "../shapes/AABB.h"
#include "../shapes/CollisionShape.h"

/// Namespace ReactPhysics3D
namespace reactphysics3d {

// Structure TreeNode
/**
 * This structure represents a node of the dynamic AABB tree.
 */
struct TreeNode {

    // -------------------- Constants -------------------- //

    /// Null tree node constant
    const static int NULL_TREE_NODE;

    // -------------------- Attributes -------------------- //

    /// Parent node ID
    int parentID;

    /// Left and right child of the node
    int leftChildID, rightChildID;

    /// Next allocated node ID
    int nextNodeID;

    /// Height of the node in the tree
    int height;

    /// Fat axis aligned bounding box (AABB) corresponding to the node
    AABB aabb;

    /// Pointer to the corresponding collision shape (in case this node is a leaf)
    CollisionShape* collisionShape;

    // -------------------- Methods -------------------- //

    /// Return true if the node is a leaf of the tree
    bool isLeaf() const;
};

// Class DynamicAABBTree
/**
 * This class implements a dynamic AABB tree that is used for broad-phase
 * collision detection. This data structure is inspired by Nathanael Presson's
 * dynamic tree implementation in BulletPhysics. The following implementation is
 * based on the one from Erin Catto in Box2D as described in the book
 * "Introduction to Game Physics with Box2D" by Ian Parberry.
 */
class DynamicAABBTree {

    private:

        // -------------------- Attributes -------------------- //

        /// Pointer to the memory location of the nodes of the tree
        TreeNode* mNodes;

        /// ID of the root node of the tree
        int mRootNodeID;

        /// ID of the first node of the list of free (allocated) nodes in the tree that we can use
        int mFreeNodeID;

        /// Number of allocated nodes in the tree
        int mNbAllocatedNodes;

        /// Number of nodes in the tree
        int mNbNodes;

        // -------------------- Methods -------------------- //

        /// Allocate and return a node to use in the tree
        int allocateNode();

        /// Release a node
        void releaseNode(int nodeID);

        /// Insert a leaf node in the tree
        void insertLeafNode(int nodeID);

        /// Remove a leaf node from the tree
        void removeLeafNode(int nodeID);

        /// Balance the sub-tree of a given node using left or right rotations.
        int balanceSubTreeAtNode(int nodeID);

    public:

        // -------------------- Methods -------------------- //

        /// Constructor
        DynamicAABBTree();

        /// Destructor
        ~DynamicAABBTree();

        /// Add an object into the tree
        int addObject(CollisionShape* collisionShape, const AABB& aabb);

        /// Remove an object from the tree
        void removeObject(int nodeID);

        /// Update the dynamic tree after an object has moved.
        bool updateObject(int nodeID, const AABB &newAABB, const Vector3 &displacement);
};

// Return true if the node is a leaf of the tree
inline bool TreeNode::isLeaf() const {
    return leftChildID == NULL_TREE_NODE;
}

}

#endif
