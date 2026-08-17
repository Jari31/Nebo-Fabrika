module Simplex3D;

import HashingFunctions;

inline float CalculateNoiseContributionOfVertex(float3 DistanceToVertex, float3 NoiseGradient)
{
    const float max_influential_radius_of_vertex = 0.6;

    float3 dot_components = NoiseGradient * DistanceToVertex;
    float dot_product = dot_components.x + dot_components.y + dot_components.z;

    float3 dist_sq_components = DistanceToVertex * DistanceToVertex;
    float vertex_distance_squared = dist_sq_components.x + dist_sq_components.y + dist_sq_components.z;

    float falloff_intensity = max(0.0f, max_influential_radius_of_vertex - vertex_distance_squared);
    float falloff_power4 = falloff_intensity * falloff_intensity * falloff_intensity * falloff_intensity;

    return falloff_power4 * dot_product;
}

public float Simplex3D(
    const float3 Origin,
    const uint Seed
)
{
    // skew the grid so you don't get the usual Perlin mess on scale and be visually be able to see the grids
    // and also to lower checks from 8 to 4 because now everything's a tetrahedron
    const float skew_factor = 1.0/3.0;
    const float unskew_factor = 1.0/6.0;

    float sum_of_input_vectors = Origin.x + Origin.y + Origin.z;
    float skew_offset = sum_of_input_vectors * skew_factor;

    float3 skew_vector = float3(skew_offset, skew_offset, skew_offset);
    float3 offset_input_vector = Origin + skew_vector;

    float3 grid_origin = float3(floor(offset_input_vector.x), floor(offset_input_vector.y), floor(offset_input_vector.z));

    float sum_of_grid_origin = grid_origin.x + grid_origin.y + grid_origin.z;
    float unskew_offset = sum_of_grid_origin * unskew_factor;

    float3 unskew_vec = float3(unskew_offset, unskew_offset, unskew_offset);
    float3 unskewed_grid_origin = grid_origin - unskew_vec;

    float3 distance_to_vertex0 = Origin - unskewed_grid_origin;

    bool X_over_Y = (distance_to_vertex0.x >= distance_to_vertex0.y);
    bool Y_over_Z = (distance_to_vertex0.y >= distance_to_vertex0.z);
    bool Z_over_X = (distance_to_vertex0.z >= distance_to_vertex0.x);

    bool X_over_Z = !Z_over_X;
    bool Y_over_X = !X_over_Y;
    bool Z_over_Y = !Y_over_Z;

    float3 step1 = float3((float)(X_over_Y && X_over_Z), (float)(Y_over_X && Y_over_Z), (float)(Z_over_X && Z_over_Y));
    float3 step2 = float3((float)(X_over_Y || X_over_Z), (float)(Y_over_X || Y_over_Z), (float)(Z_over_X || Z_over_Y));
    float3 step3 = float3(1.0f, 1.0f, 1.0f);

    float3 unskew_factor_vector = float3(unskew_factor, unskew_factor, unskew_factor);
    float3 distance_to_vertex1 = distance_to_vertex0 - step1 * unskew_factor_vector;

    float factor2 = 2.0f * unskew_factor;
    float3 factor2_vector = float3(factor2, factor2, factor2);
    float3 distance_to_vertex2 = distance_to_vertex0 - step2 * factor2_vector;

    float offset3 = 1.0f + 3.0f * unskew_factor;
    float3 offset3_vec = float3(offset3, offset3, offset3);
    float3 distance_to_vertex3 = distance_to_vertex0 - offset3_vec;

    float3 grid_origin_step_addition0 = grid_origin + step1;
    float3 grid_origin_step_addition1 = grid_origin + step2;
    float3 grid_origin_step_addition2 = grid_origin + step3;

    float3 gradient_vector0 = ScalarGradientHash(grid_origin, Seed);
    float3 gradient_vector1 = ScalarGradientHash(grid_origin_step_addition0, Seed);
    float3 gradient_vector2 = ScalarGradientHash(grid_origin_step_addition1, Seed);
    float3 gradient_vector3 = ScalarGradientHash(grid_origin_step_addition2, Seed);

    float total_noise_contributions = CalculateNoiseContributionOfVertex(distance_to_vertex0, gradient_vector0);
    total_noise_contributions += CalculateNoiseContributionOfVertex(distance_to_vertex1, gradient_vector1);
    total_noise_contributions += CalculateNoiseContributionOfVertex(distance_to_vertex2, gradient_vector2);
    total_noise_contributions += CalculateNoiseContributionOfVertex(distance_to_vertex3, gradient_vector3);

    return total_noise_contributions;
}
