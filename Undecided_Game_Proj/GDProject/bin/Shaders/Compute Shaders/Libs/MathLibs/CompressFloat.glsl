uint compress_float_normalized_uint16(float f)
{
    return uint(f * 65535.0f + 0.5);
}

float decompress_float_normalized_uint16(uint ui)
{
    return float(ui * 1.5259021896e-5);
}