# BURSTT

Assembly code for BURSTT matrix multiplication

#### frb_f32
Require AVX512, Float32 format.

#### frb_i16_v5
Reuqire AVX512, Int16 format.

#### frb_i16_v2
Reuqire AVX2, Int16 format.

#### frb_i16
Only require AVX2, Int16 format. obsolete method, matrix elelments are restricted to 255.

#### nasm
Require AVX512, Int16 format. Matrix multiplication in the slow direction.

#### nasm_f32
Require AVX512, Float32 format. Matrix multiplication in the slow direction.
