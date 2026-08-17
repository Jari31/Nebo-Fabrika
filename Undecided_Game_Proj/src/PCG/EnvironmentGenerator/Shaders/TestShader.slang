import MathLibraries.Noise.Simplex3D;

static const int THREADS_X_3D = 8;
static const int THREADS_Y_3D = 8;
static const int THREADS_Z_3D = 8;

[shader("compute")]
[numthreads(THREADS_X_3D, THREADS_Y_3D, THREADS_Z_3D)]
void main()
{
    float value = Simplex3D(float3(0, 0, 0), 0);
}
