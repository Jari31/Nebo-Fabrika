export void add_arrayss(uniform float vin[], uniform float vout[], uniform int count) {
    foreach (i = 0 ... count) {
        vout[i] = vin[i] + 2.0f;
    }
}

// copssdsa s
