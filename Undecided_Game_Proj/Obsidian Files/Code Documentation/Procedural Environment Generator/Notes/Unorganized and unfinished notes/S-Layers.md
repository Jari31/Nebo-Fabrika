To avoid floating-point precision errors, we divide the world into Sets.

There exists two set layers: Geometry and Simulation.
## Simulation Layer
The simulation layer handles the simulation at the top most layer. It starts from simple bodies (planets, asteroids), to all the way up to the fabric of the galaxy.

In other words, it goes from and to:
$Simple Body - Orbitals - Solar System - Star System - Galaxy System - Galaxy Cluster - Super Galaxy Cluster$
Or,
$S1 - S2 - S3 - S4 - S5 - S6 - S7$

In essence, it is a particle simulation. But instead of treating Sn < S7 as individual particles, we only simulate S7. And since S7 is at an unimaginable scale, we only simulate it every hour. 

Though, we do still simulate Sn < S7 layers only if there's an observer to witness it.

When Sn is simulated, it calls an event. Any children listening to that event adjust their positions as such. Then their children do the same, and so on. But Sn > S4 do not do this often - only simulation layers with a valid centroid do it. As the children of S7 are independent of centroids, as they look at particles near them, not an authoritative centroid. This is a dynamic rule.

Or, to put it simply, S7 is a box that contains other boxes (S6, S5). As such, when you move the box, the rest of the boxes move naturally. Sn < S5 are a bunch of boxes too, but they are attracted by a magnet in the middle. The magnet moves from time to time, so we need to be reminded when the magnet moves, and where.

But maybe you want to add more boxes into S6 and S5. Then you tell S6 that you are opening the box and adding more (unions between super galaxy clusters). Then S6 tells S5 that there's more boxes coming. Then S5 tells... that only happens in events; not heartbeats. Sn < S5 does it per heartbeat of every Sn.

### Light lies
In the real world, light lies to us, as certain stars that burn bright in the sky are in reality dead. As light does not travel instantly - it still takes time.

**The parent can't look down at the children, but the children can look up at the parent.** Without that rule, every layer would have to simulate every tick.

Applying that logic to our simulation, each S layer gets progressively lower in temporal resolution. As S1 might simulate itself every tick, but S2 only does it every 2 seconds, and S3 every 4, and so on. 

Whilst the children are waiting for events, they simply simulate own their own accord using the last known parameters. As it is unlikely that the sun is to move in the next few seconds in most occurrences. And errors in naturalistic simulations is a feature, not a bug. As it simulates the randomness that a computer is unable to simulate organically. 

Each Sn layer lives in its own reality. It does not know of what's outside of it. Given enough time, the galaxy you called home will move on if you were to move to another galaxy. As particles physically move.
## Geometry Layer
The geometry layer can be thought of as a probe. When a player is spawned, the geometry layer queries S7 to see if an S6 chunk contains a particle. If yes, then starts a second simulation (if there isn't one present) with the seed provided by S7. Then it queries S6, asking for particle existence in a certain chunk, and so on.

That process is regressive. As in, until we, say, enter a new S6 chunk, we don't ask S7, "is there a particle present here?" - That's incredibly slow. We simply track it.

### Layers
S1${geo}$ is the individual voxels, or building blocks, of the geometry, extracted by a GPU Dual Contouring (DC) solver. 

In S2${geo}$, it is the individual chunks. In S3${geo}$, it is the set that contains thousands to millions of S2${geo}$ sets. And this continues ad infinitum.

Geometry layers are not simulated - they are simply the world (simulation) visualized. 

----
That's the high level abstraction. But simply: we simulate a bunch of points, that represent a bunch of other points, moving around in the top layer, and then zoom in whenever we need to. As otherwise, we would have to simulate millions of trillions of points, which is physically impossible for most hardware.

## Arbitrary Precision 
Normal coordinates overflow after a while. But what do you do when you start overflowing? You flush it into a bigger bucket.

Say our S3 coordinate is INT_MAX, INT_MAX, INT_MAX. Instead of incrementing it further, we do: Counter[i + 1]++ and Counter[i] = 0. Again, if [i + 1] overflows, we recurse it. At certain points, that will drag down performance. But given we are at huge scales - it is unless we traverse at more than four times the speed of light, will we see a performance dip for more than one frame. As S3 ticks, for geometry layers, when S2 is greater than 8km. And then, again, S4 ticks only when S3 crosses INT_MAX - two billion units.

($i$ is always started at S2 in the case of geometry layers.)

This same philosophy is repeated for S layers, allowing for practically infinite worlds. This shares the same philosophy with Sparse Voxel Octrees, but in this case it's reversed. We build top down, like a fractal.

---
# The natural extension
To an extension of this idea, we can simulate economics, politics, mechanics and biology.

Each run in their own worlds, but can query eachother, and are influenced by eachother. Ships move as particles, creatures too. Policies are made from the influence of said particles.

At S2${geo}$, the terrain is a cloud of probabilities. It only collapses when you query it. At S3${geo}$, and above, it is the same. Which means, we *do not need* geometry to simulate it. We only need points and averages, and queries.

An animal can evolve offscreen, simply by using coarser and coarser resolutions the further way it is - it gives it traits you wouldn't expect from a computer simulation, as the errors build up oddities you'd see in biology. It is the survival of the fit enough, much like reality. The animals physically evolve, much like Spore, but accounted into the environment.

Economics fail because of a weird error that made two particles declare war on each other. Ships disappear, factories fail. 

Intention is art. Intentionally failing is an art of its own.