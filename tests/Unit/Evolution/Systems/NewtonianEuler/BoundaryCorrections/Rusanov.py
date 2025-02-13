# Distributed under the MIT License.
# See LICENSE.txt for details.

import numpy as np


def dg_package_data_mass_density(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    return mass_density


def dg_package_data_momentum_density(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    return momentum_density


def dg_package_data_energy_density(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    return energy_density


def dg_package_data_normal_dot_flux_mass_density(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    return np.einsum("i,i", normal_covector, flux_mass_density)


def dg_package_data_normal_dot_flux_momentum_density(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    return np.einsum("i,ij->j", normal_covector, flux_momentum_density)


def dg_package_data_normal_dot_flux_energy_density(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    return np.einsum("i,i", normal_covector, flux_energy_density)


def dg_package_data_abs_char_speed(
    mass_density,
    momentum_density,
    energy_density,
    flux_mass_density,
    flux_momentum_density,
    flux_energy_density,
    velocity,
    specific_internal_energy,
    normal_covector,
    mesh_velocity,
    normal_dot_mesh_velocity,
    use_polytropic_fluid,
):
    velocity_dot_normal = np.einsum("i,i", normal_covector, velocity)
    if use_polytropic_fluid:
        polytropic_constant = 1.0e-3
        polytropic_exponent = 2.0
        sound_speed = np.sqrt(
            polytropic_constant
            * polytropic_exponent
            * pow(mass_density, polytropic_exponent - 1.0)
        )
    else:
        adiabatic_index = 1.3
        chi = specific_internal_energy * (adiabatic_index - 1.0)
        kappa_times_p_over_rho_squared = (
            adiabatic_index - 1.0
        ) ** 2 * specific_internal_energy
        sound_speed = np.sqrt(chi + kappa_times_p_over_rho_squared)

    if normal_dot_mesh_velocity is None:
        return np.maximum(
            np.abs(velocity_dot_normal - sound_speed),
            np.abs(velocity_dot_normal + sound_speed),
        )
    else:
        return np.maximum(
            np.abs(
                velocity_dot_normal - sound_speed - normal_dot_mesh_velocity
            ),
            np.abs(
                velocity_dot_normal + sound_speed - normal_dot_mesh_velocity
            ),
        )


def dg_boundary_terms_mass_density(
    interior_mass_density,
    interior_momentum_density,
    interior_energy_density,
    interior_normal_dot_flux_mass_density,
    interior_normal_dot_flux_momentum_density,
    interior_normal_dot_flux_energy_density,
    interior_abs_char_speed,
    exterior_mass_density,
    exterior_momentum_density,
    exterior_energy_density,
    exterior_normal_dot_flux_mass_density,
    exterior_normal_dot_flux_momentum_density,
    exterior_normal_dot_flux_energy_density,
    exterior_abs_char_speed,
    use_strong_form,
):
    if use_strong_form:
        return -0.5 * (
            interior_normal_dot_flux_mass_density
            + exterior_normal_dot_flux_mass_density
        ) - 0.5 * np.maximum(
            interior_abs_char_speed, exterior_abs_char_speed
        ) * (
            exterior_mass_density - interior_mass_density
        )
    else:
        return 0.5 * (
            interior_normal_dot_flux_mass_density
            - exterior_normal_dot_flux_mass_density
        ) - 0.5 * np.maximum(
            interior_abs_char_speed, exterior_abs_char_speed
        ) * (
            exterior_mass_density - interior_mass_density
        )


def dg_boundary_terms_momentum_density(
    interior_mass_density,
    interior_momentum_density,
    interior_energy_density,
    interior_normal_dot_flux_mass_density,
    interior_normal_dot_flux_momentum_density,
    interior_normal_dot_flux_energy_density,
    interior_abs_char_speed,
    exterior_mass_density,
    exterior_momentum_density,
    exterior_energy_density,
    exterior_normal_dot_flux_mass_density,
    exterior_normal_dot_flux_momentum_density,
    exterior_normal_dot_flux_energy_density,
    exterior_abs_char_speed,
    use_strong_form,
):
    if use_strong_form:
        return -0.5 * (
            interior_normal_dot_flux_momentum_density
            + exterior_normal_dot_flux_momentum_density
        ) - 0.5 * np.maximum(
            interior_abs_char_speed, exterior_abs_char_speed
        ) * (
            exterior_momentum_density - interior_momentum_density
        )
    else:
        return 0.5 * (
            interior_normal_dot_flux_momentum_density
            - exterior_normal_dot_flux_momentum_density
        ) - 0.5 * np.maximum(
            interior_abs_char_speed, exterior_abs_char_speed
        ) * (
            exterior_momentum_density - interior_momentum_density
        )


def dg_boundary_terms_energy_density(
    interior_mass_density,
    interior_momentum_density,
    interior_energy_density,
    interior_normal_dot_flux_mass_density,
    interior_normal_dot_flux_momentum_density,
    interior_normal_dot_flux_energy_density,
    interior_abs_char_speed,
    exterior_mass_density,
    exterior_momentum_density,
    exterior_energy_density,
    exterior_normal_dot_flux_mass_density,
    exterior_normal_dot_flux_momentum_density,
    exterior_normal_dot_flux_energy_density,
    exterior_abs_char_speed,
    use_strong_form,
):
    if use_strong_form:
        return -0.5 * (
            interior_normal_dot_flux_energy_density
            + exterior_normal_dot_flux_energy_density
        ) - 0.5 * np.maximum(
            interior_abs_char_speed, exterior_abs_char_speed
        ) * (
            exterior_energy_density - interior_energy_density
        )
    else:
        return 0.5 * (
            interior_normal_dot_flux_energy_density
            - exterior_normal_dot_flux_energy_density
        ) - 0.5 * np.maximum(
            interior_abs_char_speed, exterior_abs_char_speed
        ) * (
            exterior_energy_density - interior_energy_density
        )
