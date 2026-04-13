##### This doc assumes that you already understand the [[Notes/Super Chunks|Super Chunk]] system and its related local-seed offsets.
Essentially, precompute placement positions of objects in a 1024 grid based on complex logic checks on the CPU; separate all of the axis into its own 1024 grid.

Then, on the GPU, encode Morton codes for each axis using a precomputed LUT (Look Up Table). After that, combine all of the coordinates in a {$x$, $y$ << 1, $z$ << 2} pattern. Then finally, de-interleave the coordinates, send only the points back to the CPU (using async).

1024 because the current Morton code implementation only support 10 bits per axis (as a uint can only hold 32 bits); $1024^3$ = 30 bits. And no, I don't mean a 1D LUT of size $1024^3$. Instead, I mean 3 LUTs of size 1024; each LUT only contains 1024 members. Or, all of the LUTs combined contain `1024 x 3 x sizeof(uint)` bytes of data. 

Precompute each axis at runtime based on the seed. Pattern breaking comes from the underlying noise function's terrain generation. 

# Why?
Because branching on the GPU is extremely expensive and making complex logic chains is very difficult to do (for prototyping). Take a grid of $512^3$, now imagine the CPU taking a scaled down version of that, say of size $128^3$. The CPU is still calculating each position's object value based off of a MatID and a separate noise field, which, for that many fields, is extremely expensive (not even taking into account the logic block). So, the LUT allows for the logic to be baked down into a simple memory lookup.

Alright, but you might be concerned about how we could deal with different material IDs. Simple: generate a LUT for every single variation of MatIDs. In the worst case scenario of 1000 materials (may god have mercy if you have that many for just planet terrain surface materials. i.e., grass, rocks, metals), 1000 x (1024 x 3 x UINT_SIZE) = 1000(1024 x 3 x 4) = 12,288,000 bytes. Or, 12.288 MBs. As a reference, a single low resolution terrain-gen pipeline is ***120 mbs***.

***CPU logic should be async.*** Also, crucial detail: the seed the precomputation table uses is the *global seed*. The CPU will only ever need to recalculate if the global seed changes (which shouldn't; just modify the local seed) or if a new environment material is added. Alongside that, generated points are sparse.

# More detailed and controlled LUTs
Alright, now you know the why. So let's move onto something a bit more complex.
We need two different fields, each with  3 1-member arrays:

Assume,
uint i = MortonLUT($Position_{n}$) * MatID (an offset)

Where,
ObjectID = MAT[i]

Base XYZ to 
int Material[] = ObjectID; 
$n_{MAT[]}$ = ObjectID$_{n}$
MortonInterleave($X_{MAT[]}$, $Y_{MAT[]}$, $Z_{MAT[]}$) = ObjectID;

You can simply keep appending more precomputations into that 1D array (in order so MatID offsets can function), as it shall remain dynamic.

Base:
Normal - Expand the normal into 0 - 1023, where the first 511 bits can represent the minus sign, the 512th can represent the 0 and the latter 511b can represent the plus sign), 

Elevation - Similarly, use a logarithmic scale or something similar to clamp it to 0 - 1023 with similar signatures, 

Distance - Can be any variable, really. But again, use a log scale 

-to Material2[] = FractionOfObjectID; (FOOID)

Where FOOID = an int that has the approximate and quantized value of 3 different parameters 
Simply add the FOOID to the ObjectID to get an approximate value. 

$n_{MAT2[]}$ = FOOID$_{n}$
MortonInterleave($N_{MAT2[]}$, $E_{MAT2[]}$, $D_{MAT2[]}$) = FOOID

End result = FOOID + ObjectID
# CPU Logic
Okay, so that's a lot of talk about using the LUT data. But how do we generate the LUT data itself? Well, it depends on your branching. 

Since it is so ambiguous, I will only give a general overview. 

Calculating a noise field, referencing a MatID and using only a single axis - that's for you to figure out. Regardless, store the data that you calculated in a 1D array; have 1024 nodes. Crucially, store the data as an integer.

Now we come to the NED field (Normal, Elevation, Distance). How do we calculate it? It's actually pretty simple. Basically, have a branch for each of those elements. 

``` cpp
float contribution[];
void branch(logic N)
{
	for(int i = 0)
	{
		... // logic. 
			// check for every single normal configuration in a 1024 range
			// where former 511 configs = -1 to -0.1
			// middle 512th config = 0
			// latter 511 configs = 0.1 to 1 
	
		contribution[i] += computeNormal;
	}
}

generateLUT_1D(contribution)
```

And yeah, that's basically the entire logic behind this. You can pretty much do the exact same thing for the E (Elevation) and D (Distance) elements of the NED field.