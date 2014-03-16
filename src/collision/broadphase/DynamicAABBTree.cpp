/********************************************************************************
* ReactPhysics3D physics library, http://code.google.com/p/reactphysics3d/      *
* Copyright (c) 2010-2013 Daniel Chappuis                                       *
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

// Libraries
#include "DynamicAABBTree.h"

using namespace reactphysics3d;

// Initialization of static variables
const int TreeNode::NULL_TREE_NODE = -1;

// Constructor
DynamicAABBTree::DynamicAABBTree() {

    mRootNodeID = TreeNode::NULL_TREE_NODE;
    mNbNodes = 0;
    mNbAllocatedNodes = 8;

    // Allocate memory for the nodes of the tree
    mNodes = (TreeNode*) malloc(mNbAllocatedNodes * sizeof(TreeNode));
    assert(mNodes);
    memset(mNodes, 0, mNbAllocatedNodes * sizeof(TreeNode));

    // Initialize the allocated nodes
    for (int i=0; i<mNbAllocatedNodes - 1; i++) {
        mNodes[i].nextNodeID = i + 1;
        mNodes[i].height = -1;
    }
    mNodes[mNbAllocatedNodes - 1].nextNodeID = TreeNode::NULL_TREE_NODE;
    mNodes[mNbAllocatedNodes - 1].height = -1;
    mFreeNodeID = 0;
}

// Destructor
DynamicAABBTree::~DynamicAABBTree() {

    // Free the allocated memory for the nodes
    free(mNodes);
}

// Allocate and return a new node in the tree
int DynamicAABBTree::allocateNode() {

    // If there is no more allocated node to use
    if (mFreeNodeID == TreeNode::NULL_TREE_NODE) {

        assert(mNbNodes == mNbAllocatedNodes);

        // Allocate more nodes in the tree
        mNbAllocatedNodes *= 2;
        TreeNode* oldNodes = mNodes;
        mNodes = (TreeNode*) malloc(mNbAllocatedNodes * sizeof(TreeNode));
        assert(mNodes);
        memcpy(mNodes, oldNodes, mNbNodes * sizeof(TreeNode));
        free(oldNodes);

        // Initialize the allocated nodes
        for (int i=mNbNodes; i<mNbAllocatedNodes - 1; i++) {
            mNodes[i].nextNodeID = i + 1;
            mNodes[i].height = -1;
        }
        mNodes[mNbAllocatedNodes - 1].nextNodeID = TreeNode::NULL_TREE_NODE;
        mNodes[mNbAllocatedNodes - 1].height = -1;
        mFreeNodeID = mNbNodes;
    }

    // Get the next free node
    int freeNodeID = mFreeNodeID;
    mFreeNodeID = mNodes[freeNodeID].nextNodeID;
    mNodes[freeNodeID].parentID = TreeNode::NULL_TREE_NODE;
    mNodes[freeNodeID].leftChildID = TreeNode::NULL_TREE_NODE;
    mNodes[freeNodeID].rightChildID = TreeNode::NULL_TREE_NODE;
    mNodes[freeNodeID].collisionShape = NULL;
    mNodes[freeNodeID].height = 0;
    mNbNodes++;

    return freeNodeID;
}

// Release a node
void DynamicAABBTree::releaseNode(int nodeID) {

    assert(mNbNodes > 0);
    assert(nodeID >= 0 && nodeID < mNbAllocatedNodes);
    mNodes[nodeID].nextNodeID = mFreeNodeID;
    mNodes[nodeID].height = -1;
    mFreeNodeID = nodeID;
    mNbNodes--;

    // Deallocate nodes memory here if the number of allocated nodes is large
    // compared to the number of nodes in the tree
    if ((mNbNodes < mNbAllocatedNodes / 4) && mNbNodes > 8) {

        // Allocate less nodes in the tree
        mNbAllocatedNodes /= 2;
        TreeNode* oldNodes = mNodes;
        mNodes = (TreeNode*) malloc(mNbAllocatedNodes * sizeof(TreeNode));
        assert(mNodes);
        memcpy(mNodes, oldNodes, mNbNodes * sizeof(TreeNode));
        free(oldNodes);

        // Initialize the allocated nodes
        for (int i=mNbNodes; i<mNbAllocatedNodes - 1; i++) {
            mNodes[i].nextNodeID = i + 1;
            mNodes[i].height = -1;
        }
        mNodes[mNbAllocatedNodes - 1].nextNodeID = TreeNode::NULL_TREE_NODE;
        mNodes[mNbAllocatedNodes - 1].height = -1;
        mFreeNodeID = mNbNodes;
    }
}

// Add an object into the tree. This method creates a new leaf node in the tree and
// returns the ID of the corresponding node.
int DynamicAABBTree::addObject(CollisionShape* collisionShape, const AABB& aabb) {

    // Get the next available node (or allocate new ones if necessary)
    int nodeID = allocateNode();

    // Create the fat aabb to use in the tree
    const Vector3 gap(DYNAMIC_TREE_AABB_GAP, DYNAMIC_TREE_AABB_GAP, DYNAMIC_TREE_AABB_GAP);
    mNodes[nodeID].aabb.setMin(mNodes[nodeID].aabb.getMin() - gap);
    mNodes[nodeID].aabb.setMax(mNodes[nodeID].aabb.getMax() + gap);

    // Set the collision shape
    mNodes[nodeID].collisionShape = collisionShape;

    // Set the height of the node in the tree
    mNodes[nodeID].height = 0;

    // Insert the new leaf node in the tree
    insertLeafNode(nodeID);

    // Return the node ID
    return nodeID;
}

// Remove an object from the tree
void DynamicAABBTree::removeObject(int nodeID) {

    assert(nodeID >= 0 && nodeID < mNbAllocatedNodes);
    assert(mNodes[nodeID].isLeaf());

    // Remove the node from the tree
    removeLeafNode(nodeID);
    releaseNode(nodeID);
}

// Update the dynamic tree after an object has moved.
/// If the new AABB of the object that has moved is still inside its fat AABB, then
/// nothing is done. Otherwise, the corresponding node is removed and reinserted into the tree.
/// The method returns true if the object has been reinserted into the tree.
bool DynamicAABBTree::updateObject(int nodeID, const AABB& newAABB, const Vector3& displacement) {

    assert(nodeID >= 0 && nodeID < mNbAllocatedNodes);
    assert(mNodes[nodeID].isLeaf());

    // If the new AABB is still inside the fat AABB of the node
    if (mNodes[nodeID].aabb.contains(newAABB)) {
        return false;
    }

    // If the new AABB is outside the fat AABB, we remove the corresponding node
    removeLeafNode(nodeID);

    // Compute a new fat AABB for the new AABB by taking the object displacement into account
    AABB fatAABB = newAABB;
    const Vector3 gap(DYNAMIC_TREE_AABB_GAP, DYNAMIC_TREE_AABB_GAP, DYNAMIC_TREE_AABB_GAP);
    fatAABB.mMinCoordinates -= gap;
    fatAABB.mMaxCoordinates += gap;
    const Vector3 displacementGap = AABB_DISPLACEMENT_MULTIPLIER * displacement;
    if (displacementGap.x < decimal(0.0)) {
        fatAABB.mMinCoordinates.x += displacementGap.x;
    }
    else {
        fatAABB.mMaxCoordinates.x += displacementGap.x;
    }
    if (displacementGap.y < decimal(0.0)) {
        fatAABB.mMinCoordinates.y += displacementGap.y;
    }
    else {
        fatAABB.mMaxCoordinates.y += displacementGap.y;
    }
    if (displacementGap.z < decimal(0.0)) {
        fatAABB.mMinCoordinates.z += displacementGap.z;
    }
    else {
        fatAABB.mMaxCoordinates.z += displacementGap.z;
    }
    mNodes[nodeID].aabb = fatAABB;

    // Reinsert the node into the tree
    insertLeafNode(nodeID);

    return true;
}

// Insert a leaf node in the tree. The process of inserting a new leaf node
// in the dynamic tree is described in the book "Introduction to Game Physics
// with Box2D" by Ian Parberry.
void DynamicAABBTree::insertLeafNode(int nodeID) {

    // If the tree is empty
    if (mRootNodeID == TreeNode::NULL_TREE_NODE) {
        mRootNodeID = nodeID;
        mNodes[mRootNodeID].parentID = TreeNode::NULL_TREE_NODE;
        return;
    }

    // Find the best sibling node for the new node
    AABB newNodeAABB = mNodes[nodeID].aabb;
    int currentNodeID = mRootNodeID;
    while (!mNodes[currentNodeID].isLeaf()) {

        int leftChild = mNodes[currentNodeID].leftChildID;
        int rightChild = mNodes[currentNodeID].rightChildID;

        // Compute the merged AABB
        decimal volumeAABB = mNodes[currentNodeID].aabb.getVolume();
        AABB mergedAABBs;
        mergedAABBs.mergeTwoAABBs(mNodes[currentNodeID].aabb, newNodeAABB);
        decimal mergedVolume = mergedAABBs.getVolume();

        // Compute the cost of making the current node the sibbling of the new node
        decimal costS = decimal(2.0) * mergedVolume;

        // Compute the minimum cost of pushing the new node further down the tree (inheritance cost)
        decimal costI = decimal(2.0) * (mergedVolume - volumeAABB);

        // Compute the cost of descending into the left child
        decimal costLeft;
        AABB currentAndLeftAABB;
        currentAndLeftAABB.mergeTwoAABBs(newNodeAABB, mNodes[leftChild].aabb);
        if (mNodes[leftChild].isLeaf()) {   // If the left child is a leaf
            costLeft = currentAndLeftAABB.getVolume() + costI;
        }
        else {
            decimal leftChildVolume = mNodes[leftChild].aabb.getVolume();
            costLeft = costI + currentAndLeftAABB.getVolume() - leftChildVolume;
        }

        // Compute the cost of descending into the right child
        decimal costRight;
        AABB currentAndRightAABB;
        currentAndRightAABB.mergeTwoAABBs(newNodeAABB, mNodes[rightChild].aabb);
        if (mNodes[rightChild].isLeaf()) {   // If the right child is a leaf
            costRight = currentAndRightAABB.getVolume() + costI;
        }
        else {
            decimal rightChildVolume = mNodes[rightChild].aabb.getVolume();
            costRight = costI + currentAndRightAABB.getVolume() - rightChildVolume;
        }

        // If the cost of making the current node a sibbling of the new node is smaller than
        // the cost of going down into the left or right child
        if (costS < costLeft && costS < costRight) break;

        // It is cheaper to go down into a child of the current node, choose the best child
        if (costLeft < costRight) {
            currentNodeID = leftChild;
        }
        else {
            currentNodeID = rightChild;
        }
    }

    int siblingNode = currentNodeID;

    // Create a new parent for the new node and the sibling node
    int oldParentNode = mNodes[siblingNode].parentID;
    int newParentNode = allocateNode();
    mNodes[newParentNode].parentID = oldParentNode;
    mNodes[newParentNode].collisionShape = NULL;
    mNodes[newParentNode].aabb.mergeTwoAABBs(mNodes[siblingNode].aabb, newNodeAABB);
    mNodes[newParentNode].height = mNodes[siblingNode].height + 1;

    // If the sibling node was not the root node
    if (oldParentNode != TreeNode::NULL_TREE_NODE) {
        if (mNodes[oldParentNode].leftChildID == siblingNode) {
            mNodes[oldParentNode].leftChildID = newParentNode;
        }
        else {
            mNodes[oldParentNode].rightChildID = newParentNode;
        }
    }
    else {  // If the sibling node was the root node
        mNodes[newParentNode].leftChildID = siblingNode;
        mNodes[newParentNode].rightChildID = nodeID;
        mNodes[siblingNode].parentID = newParentNode;
        mNodes[nodeID].parentID = newParentNode;
        mRootNodeID = newParentNode;
    }

    // Move up in the tree to change the AABBs that have changed
    currentNodeID = mNodes[nodeID].parentID;
    while (currentNodeID != TreeNode::NULL_TREE_NODE) {

        // Balance the sub-tree of the current node if it is not balanced
        currentNodeID = balanceSubTreeAtNode(currentNodeID);

        int leftChild = mNodes[currentNodeID].leftChildID;
        int rightChild = mNodes[currentNodeID].rightChildID;
        assert(leftChild != TreeNode::NULL_TREE_NODE);
        assert(rightChild != TreeNode::NULL_TREE_NODE);

        // Recompute the height of the node in the tree
        mNodes[currentNodeID].height = std::max(mNodes[leftChild].height,
                                                mNodes[rightChild].height) + 1;

        // Recompute the AABB of the node
        mNodes[currentNodeID].aabb.mergeTwoAABBs(mNodes[leftChild].aabb, mNodes[rightChild].aabb);

        currentNodeID = mNodes[currentNodeID].parentID;
    }
}

// Remove a leaf node from the tree
void DynamicAABBTree::removeLeafNode(int nodeID) {

    assert(nodeID >= 0 && nodeID < mNbAllocatedNodes);

    // If we are removing the root node (root node is a leaf in this case)
    if (mRootNodeID == nodeID) {
        mRootNodeID = TreeNode::NULL_TREE_NODE;
        return;
    }

    int parentNodeID = mNodes[nodeID].parentID;
    int grandParentNodeID = mNodes[parentNodeID].parentID;
    int siblingNodeID;
    if (mNodes[parentNodeID].leftChildID == nodeID) {
        siblingNodeID = mNodes[parentNodeID].rightChildID;
    }
    else {
        siblingNodeID = mNodes[parentNodeID].leftChildID;
    }

    // If the parent of the node to remove is not the root node
    if (grandParentNodeID != TreeNode::NULL_TREE_NODE) {

        // Destroy the parent node
        if (mNodes[grandParentNodeID].leftChildID == parentNodeID) {
            mNodes[grandParentNodeID].leftChildID = siblingNodeID;
        }
        else {
            assert(mNodes[grandParentNodeID].rightChildID == parentNodeID);
            mNodes[grandParentNodeID].rightChildID = siblingNodeID;
        }
        mNodes[siblingNodeID].parentID = grandParentNodeID;
        releaseNode(parentNodeID);

        // Now, we need to recompute the AABBs of the node on the path back to the root
        // and make sure that the tree is still balanced
        int currentNodeID = grandParentNodeID;
        while(currentNodeID != TreeNode::NULL_TREE_NODE) {

            // Balance the current sub-tree if necessary
            currentNodeID = balanceSubTreeAtNode(currentNodeID);

            // Get the two children of the current node
            int leftChildID = mNodes[currentNodeID].leftChildID;
            int rightChildID = mNodes[currentNodeID].rightChildID;

            // Recompute the AABB and the height of the current node
            mNodes[currentNodeID].aabb.mergeTwoAABBs(mNodes[leftChildID].aabb,
                                                     mNodes[rightChildID].aabb);
            mNodes[currentNodeID].height = std::max(mNodes[leftChildID].height,
                                                    mNodes[rightChildID].height) + 1;

            currentNodeID = mNodes[currentNodeID].parentID;
        }

    }
    else { // If the parent of the node to remove is the root node

        // The sibling node becomes the new root node
        mRootNodeID = siblingNodeID;
        mNodes[siblingNodeID].parentID = TreeNode::NULL_TREE_NODE;
        releaseNode(nodeID);
    }
}

// Balance the sub-tree of a given node using left or right rotations.
/// The rotation schemes are described in in the book "Introduction to Game Physics
/// with Box2D" by Ian Parberry. This method returns the new root node ID.
int DynamicAABBTree::balanceSubTreeAtNode(int nodeID) {

    assert(nodeID != TreeNode::NULL_TREE_NODE);

    TreeNode* nodeA = mNodes + nodeID;

    // If the node is a leaf or the height of A's sub-tree is less than 2
    if (nodeA->isLeaf() || nodeA->height < 2) {

        // Do not perform any rotation
        return nodeID;
    }

    // Get the two children nodes
    int nodeBID = nodeA->leftChildID;
    int nodeCID = nodeA->rightChildID;
    assert(nodeBID >= 0 && nodeBID < mNbAllocatedNodes);
    assert(nodeCID >= 0 && nodeCID < mNbAllocatedNodes);
    TreeNode* nodeB = mNodes + nodeBID;
    TreeNode* nodeC = mNodes + nodeCID;

    // Compute the factor of the left and right sub-trees
    int balanceFactor = nodeC->height - nodeB->height;

    // If the right node C is 2 higher than left node B
    if (balanceFactor > 1) {

        int nodeFID = nodeC->leftChildID;
        int nodeGID = nodeC->rightChildID;
        assert(nodeFID >= 0 && nodeFID < mNbAllocatedNodes);
        assert(nodeGID >= 0 && nodeGID < mNbAllocatedNodes);
        TreeNode* nodeF = mNodes + nodeFID;
        TreeNode* nodeG = mNodes + nodeGID;

        nodeC->leftChildID = nodeID;
        nodeC->parentID = nodeA->parentID;
        nodeA->parentID = nodeCID;

        if (nodeC->parentID != TreeNode::NULL_TREE_NODE) {

            if (mNodes[nodeC->parentID].leftChildID == nodeID) {
                mNodes[nodeC->parentID].leftChildID = nodeCID;
            }
            else {
                assert(mNodes[nodeC->parentID].rightChildID == nodeID);
                mNodes[nodeC->parentID].rightChildID = nodeCID;
            }
        }
        else {
            mRootNodeID = nodeCID;
        }

        // If the right node C was higher than left node B because of the F node
        if (nodeF->height > nodeG->height) {
            nodeC->rightChildID = nodeFID;
            nodeA->rightChildID = nodeGID;
            nodeG->parentID = nodeID;

            // Recompute the AABB of node A and C
            nodeA->aabb.mergeTwoAABBs(nodeB->aabb, nodeG->aabb);
            nodeC->aabb.mergeTwoAABBs(nodeA->aabb, nodeF->aabb);

            // Recompute the height of node A and C
            nodeA->height = std::max(nodeB->height, nodeG->height) + 1;
            nodeC->height = std::max(nodeA->height, nodeF->height) + 1;
        }
        else {  // If the right node C was higher than left node B because of node G
            nodeC->rightChildID = nodeGID;
            nodeA->rightChildID = nodeFID;
            nodeF->parentID = nodeID;

            // Recompute the AABB of node A and C
            nodeA->aabb.mergeTwoAABBs(nodeB->aabb, nodeF->aabb);
            nodeC->aabb.mergeTwoAABBs(nodeA->aabb, nodeG->aabb);

            // Recompute the height of node A and C
            nodeA->height = std::max(nodeB->height, nodeF->height) + 1;
            nodeC->height = std::max(nodeA->height, nodeG->height) + 1;
        }

        // Return the new root of the sub-tree
        return nodeCID;
    }

    // If the left node B is 2 higher than right node C
    if (balanceFactor < -1) {

        int nodeFID = nodeB->leftChildID;
        int nodeGID = nodeB->rightChildID;
        assert(nodeFID >= 0 && nodeFID < mNbAllocatedNodes);
        assert(nodeGID >= 0 && nodeGID < mNbAllocatedNodes);
        TreeNode* nodeF = mNodes + nodeFID;
        TreeNode* nodeG = mNodes + nodeGID;

        nodeB->leftChildID = nodeID;
        nodeB->parentID = nodeA->parentID;
        nodeA->parentID = nodeBID;

        if (nodeB->parentID != TreeNode::NULL_TREE_NODE) {

            if (mNodes[nodeB->parentID].leftChildID == nodeID) {
                mNodes[nodeB->parentID].leftChildID = nodeBID;
            }
            else {
                assert(mNodes[nodeB->parentID].rightChildID == nodeID);
                mNodes[nodeB->parentID].rightChildID = nodeBID;
            }
        }
        else {
            mRootNodeID = nodeBID;
        }

        // If the left node B was higher than right node C because of the F node
        if (nodeF->height > nodeG->height) {
            nodeB->rightChildID = nodeFID;
            nodeA->leftChildID = nodeGID;
            nodeG->parentID = nodeID;

            // Recompute the AABB of node A and B
            nodeA->aabb.mergeTwoAABBs(nodeC->aabb, nodeG->aabb);
            nodeB->aabb.mergeTwoAABBs(nodeA->aabb, nodeF->aabb);

            // Recompute the height of node A and B
            nodeA->height = std::max(nodeC->height, nodeG->height) + 1;
            nodeB->height = std::max(nodeA->height, nodeF->height) + 1;
        }
        else {  // If the left node B was higher than right node C because of node G
            nodeB->rightChildID = nodeGID;
            nodeA->leftChildID = nodeFID;
            nodeF->parentID = nodeID;

            // Recompute the AABB of node A and B
            nodeA->aabb.mergeTwoAABBs(nodeC->aabb, nodeF->aabb);
            nodeB->aabb.mergeTwoAABBs(nodeA->aabb, nodeG->aabb);

            // Recompute the height of node A and B
            nodeA->height = std::max(nodeC->height, nodeF->height) + 1;
            nodeB->height = std::max(nodeA->height, nodeG->height) + 1;
        }

        // Return the new root of the sub-tree
        return nodeBID;
    }

    // If the sub-tree is balanced, return the current root node
    return nodeID;
}
