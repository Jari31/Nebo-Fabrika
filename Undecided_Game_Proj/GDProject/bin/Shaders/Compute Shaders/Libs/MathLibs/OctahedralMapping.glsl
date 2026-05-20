vec2 pack_normal_oct(vec3 Normal)
{
    vec2 p = Normal.xy / (abs(Normal.x) + abs(Normal.y) + abs(Normal.z));
    
    if(Normal.z <= 0.0)
        p = (1.0 - abs(p.yx)) * vec2(Normal.x >= 0.0 ? 1.0 : -1.0, Normal.y >= 0.0 ? 1.0 : -1.0);

    return p;
}

vec3 unpack_normal_oct(vec2 Point)
{
    vec3 n = vec3(Point.x, Point.y, 1.0 - abs(Point.x) - abs(Point.y));
    if(n.z < 0.0)
    {
        vec2 _sign = vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
        n.xy = (1.0 - abs(n.yx)) * _sign;
    }

    return normalize(n);
}