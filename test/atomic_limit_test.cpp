#include <alps/params.hpp>
#include <gtest/gtest.h>

#include "opendf/lattice_traits.hpp"
#include "opendf/df.hpp"

using namespace open_df;

static constexpr int D = 2; // work in 2 dimensions
typedef cubic_traits<D> lattice_t; 
typedef df_hubbard<lattice_t> df_type; 

typedef df_type::vertex_type vertex_type;
typedef df_type::gw_type gw_type;
typedef df_type::gk_type gk_type;
typedef df_type::disp_type disp_type;

int main(int argc, char *argv[])
{
    alps::params p; 

    double U = 16; 
    double beta = 1.0;
    double mu = U/2;
    int wmax = 4;

    typedef std::complex<double> complex_type;
    typedef typename fmatsubara_grid::point fpoint; 
    typedef typename bmatsubara_grid::point bpoint; 

    // grids
    fmatsubara_grid fgrid(-wmax,wmax,beta);
    bmatsubara_grid bgrid(-1,2,beta);
    DEBUG(bgrid);
        
    auto mult = beta*U*U/4.;
    gw_type Lambda(fgrid);
    typename gw_type::function_type lambdaf = [mult,U](std::complex<double> w){return 1. - U*U/4./w/w;};
    Lambda.fill(lambdaf);

    // magnetic vertex
    vertex_type magnetic_vertex(std::forward_as_tuple(bgrid,fgrid,fgrid));
    vertex_type::point_function_type VertexF;
    VertexF = [&](bpoint W,fpoint w1, fpoint w2)->std::complex<double>{
            return  double(is_float_equal(W.value(), 0))*mult*Lambda(w1)*Lambda(w2)*(2. + double(w1.index() == w2.index()));
    };
    magnetic_vertex.fill(VertexF);

    // density vertex
    vertex_type density_vertex(magnetic_vertex.grids());
    density_vertex = 0;
    VertexF = [&](bpoint W,fpoint w1, fpoint w2)->std::complex<double>{
            return  double(is_float_equal(W.value(), 0))*mult*Lambda(w1)*Lambda(w2)*(-3.0)*double(w1.index() == w2.index());
    };
    //density_vertex = mult*Lambda*Lambda*(-3.);  // Triplet vertex S_z = 1, S = 1
    density_vertex.fill(VertexF);

    // local gf
    std::function<complex_type(fpoint)> gw_f = [U](fpoint w) { return 0.5 / (w.value() - U/2.) + 0.5 / (w.value() + U/2.); };
    gw_type gw(fgrid);
    gw.fill(gw_f);

    gw_type delta(gw.grids());
    delta = gw * 2 * double(D);

    gw.savetxt("gw_test.dat");
    delta.savetxt("delta_test.dat");
    
    // parameters
    double hopping_t = 1.0;
    double T = 1.0/beta;
    int kpts = 16;

    p["df_sc_mix"] = 1.0;
    p["df_sc_iter"] = 1;
    p["nbosonic"] = 1;
    //std::cout << p << std::flush;
    std::cout << "temperature = " << T << std::endl;
    std::cout << std::endl;

    // create a grid over Brilloin zone for evaluation 
    kmesh kgrid(kpts);
    
    // define lattice
    lattice_t lattice(hopping_t);
    // get dispersion
    disp_type disp(std::forward_as_tuple(kgrid, kgrid));
    disp.fill(lattice.get_dispersion());  
        
    // construct a df run 
    df_hubbard<cubic_traits<2>> DF(gw, delta, lattice, kgrid, density_vertex, magnetic_vertex);
    // run df
    gw_type delta_upd = DF(p);
    gw_type glat_loc = DF.glat_loc();
    glat_loc.savetxt("gloc_test.dat");

    gw_type gloc_comp(fgrid);
    std::vector<std::complex<double>> v = 
        {{ 4.014511542792e-02, 5.049725242672e-02, 6.137306974871e-02, 4.125122660845e-02, 
          -4.125122660845e-02, -6.137306974871e-02, -5.049725242672e-02, -4.014511542792e-02 }};
    std::copy(v.begin(), v.end(), gloc_comp.data().data());
    gloc_comp *= I;
    std::cout << "local gf comparison : " << std::endl;
    std::cout << glat_loc << "\n == \n" << gloc_comp << std::endl;
    double diff = glat_loc.diff(gloc_comp);
    std::cout << "diff = " << diff << std::endl;
    EXPECT_NEAR(diff, 0, 1e-5);
}
