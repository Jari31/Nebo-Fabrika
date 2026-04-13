Dual contouring is a *meshing algorithm*. It takes a cell (voxel), checks every edge to see if an edge changes its sign on either end of the edge and then computes a QEF function to determine where to place a vertex. Afterwards, it checks its neighbors to understand how to generate a quad in order to form a mesh.

There are currently two implementations of this algorithm present within the game: Volatile, sparse DC and dense DC.

# Sparse DC
