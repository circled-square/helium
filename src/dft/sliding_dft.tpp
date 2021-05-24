namespace dft {
    template<typename T, u32 N, class dr>
    std::array<std::complex<T>, N>
    sliding_dft<T,N,dr>::harmonic_phase;

    template<typename T, u32 N, class dr>
    bool sliding_dft<T,N,dr>::static_attributes_are_inited = false;
}