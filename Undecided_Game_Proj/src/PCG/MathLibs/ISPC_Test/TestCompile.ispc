export void add_arrays(uniform float vin[], uniform float vout[], uniform int count) {
    foreach (i = 0 ... count) {
        vout[i] = vin[i] + 1.0f;
    }
}

// copsssd s
