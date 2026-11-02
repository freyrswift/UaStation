# Local Reproduction

Confirm the following on the page side before returning to Node:

- The real entry function
- Call order
- Parameter sources
- Browser objects depended on
- Whether it depends on time, random numbers, storage, cookies, UA, canvas, crypto

Do a minimal reproduction first, then patch the environment step by step; do not simulate the whole browser in one shot.
