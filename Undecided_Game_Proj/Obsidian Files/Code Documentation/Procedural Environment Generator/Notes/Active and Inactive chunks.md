[[Notes/Dense Chunks (Active)|Dense Chunks (Active)]] and [[Sparse Chunks (Inactive)]] are systems meant to display virtually infinite chunks whilst only being limited by vertices and player perception. Both of these systems are GPU based and run on compute shaders.

Dense chunks are meant to be simpler noise fields that do not do *[[Notes/Sparse Chunks (Inactive)|SVO construction]]* and turn into an [[Sparse Chunks (Inactive)|SVO]]. They are considered active because they are *[[Async|non-asynchronous]]*. As they are the only chunk where physics is being actively calculated.

SVO chunks, however, work in a different way. In essence, the player shoots out probes based on their Field of Vision (FoV) to activate [[Notes/Super Chunks|Super Chunks]]. This is called [[Notes/Explorative Generation|explorative generation]].

Note: *Physics is calculated on the mesh (vertices); not the voxels themselves*. Empty space is not calculated on. Alongside, meshes are decimated and simplified for physics (physics is handled by Godot; the system simply feeds the mesh into Godot to handle). Volumetric physics is simply too time consuming to implement to be viable.

# As a scale of reference: 
$12^3$ is the average amount of chunks a player in Minecraft sees all around them. That itself there is the active chunk size for this game. The inactive SVO chunks are also $12^3$ chunks big. You can now probably imagine how huge a single probe is. 

You can imagine the voxels as the atoms, chunks as cells, super chunks as tissues, planets as multi-celled organisms, solar systems as societies, and so on and so forth.