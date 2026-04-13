The active dense chunk should first run at a very low resolution and mesh itself. The mesh should then be used as a physics proxy until the high resolution mesh finishes generating.

Although, decimating the high res mesh so the CPU can use it might be not worth the while. Just use the low resolution mesh. It should run every time the chunk is modified. It should be synchronous. It should have its own pipeline and run at its own pace. It should serve as a preview whilst the high res pass is generating.

The high res pass should be asynchronous and run at its own pace in its own pipeline. It should submit its vertex data right into the rendering pipeline after registering its vertices with materials by an uber shader.

In total, reserve 4 buffers. Two for the active pass and two for the passive pass.

Two raycasters, where one generates the preview chunks very fast, meanwhile the other takes its time generating the SVO and displaying it.

The CPU should run a check against the planet's sphere SDF to check if it should generate the super chunk. Alongside that, the grid should not go on forever.

```dataviewjs
const el = this.container;
```