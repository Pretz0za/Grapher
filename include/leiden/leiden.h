// Assumptions:
//	a- static graph


// partitioning:
// struct partition {
//	u16/u32 *assignments (N elements, each vertex gets its community)
//	internalEdgeCount* (stores how many internal edges are in each community)
//	number of communities
// }
//
//OPERATIONS:
// operation P(v → C), add vertex to community: O(1) just change the assignment in partition.
// operation make singleton: return partition with harmonic sequence as assignments
// membership checking for specific vertex: trivial, check assignments array
// iterating over a community: O(N) must iterate over whole array. Can resort to storing a positionInCommunity N element array. 
// 
//Initializing:
//	
