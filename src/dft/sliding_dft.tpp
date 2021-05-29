namespace dft {
    template<std::floating_point T, u32 N, scluk::concepts::ratio dr>
    std::array<std::complex<T>, N> sliding_dft<T,N,dr>::harmonic_phase;

    template<std::floating_point T, u32 N, scluk::concepts::ratio dr>
    bool sliding_dft<T,N,dr>::static_attributes_are_inited = false;
}