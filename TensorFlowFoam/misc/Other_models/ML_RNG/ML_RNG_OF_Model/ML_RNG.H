/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::RASModels::ML_RNG

Group
    grpRASTurbulence

Description
    Openfoam Machine learning model - Romit Maulik

SourceFiles
    ML_RNG.C

\*---------------------------------------------------------------------------*/

#ifndef ML_RNG_H
#define ML_RNG_H

#include "RASModel.H"
#include "eddyViscosity.H"
#include "tf_utils.H" // Loads in tensorflow functionality

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace RASModels
{

/*---------------------------------------------------------------------------*\
                      Class ML_RNG Declaration
\*---------------------------------------------------------------------------*/

template<class BasicTurbulenceModel>
class ML_RNG
:
    public eddyViscosity<RASModel<BasicTurbulenceModel>>
{
    // Private Member Functions

        // Disallow default bitwise copy construct and assignment
        ML_RNG(const ML_RNG&);
        void operator=(const ML_RNG&);


protected:

    // Protected data

        // Model coefficients

            dimensionedScalar sigmaNut_;
            dimensionedScalar kappa_;

            dimensionedScalar Cb1_;
            dimensionedScalar Cb2_;
            dimensionedScalar Cw1_;
            dimensionedScalar Cw2_;
            dimensionedScalar Cw3_;
            dimensionedScalar Cv1_;
            dimensionedScalar Cs_;

            // Tensorflow graph operations - protected members of this class - top three not used in constructor definition since they don't have default values
            TF_Graph* graph_;
            TF_Output input_ph_;
            TF_Output output_;

            // Fields
            volScalarField nuTilda_;
            volScalarField umag_;
            volScalarField gradu_;
            volScalarField gradv_;
            volScalarField rot_;
            volScalarField vortz_;
            volScalarField nut_ml_;

            //- Wall distance
            //  Note: different to wall distance in parent RASModel
            //  which is for near-wall cells only
            const volScalarField& y_;

            // LES filters
            simpleFilter MyFilter_;
            // autoPtr<LESfilter> filterPtr_;
            // LESfilter& filter_;

            // OpenMP num threads
            int omp_num_threads_;


            

    // Protected Member Functions

        tmp<volScalarField> chi() const;

        tmp<volScalarField> fv1(const volScalarField& chi) const;

        tmp<volScalarField> fv2
        (
            const volScalarField& chi,
            const volScalarField& fv1
        ) const;

        tmp<volScalarField> Stilda
        (
            const volScalarField& chi,
            const volScalarField& fv1
        ) const;

        tmp<volScalarField> fw(const volScalarField& Stilda) const;

        void correctNut(const volScalarField& fv1);
        virtual void correctNut();

        //- run the ML graph
        void run_ml_graph(double* mean_array, double* std_array, int num_inputs, int num_outputs);


public:

    typedef typename BasicTurbulenceModel::alphaField alphaField;
    typedef typename BasicTurbulenceModel::rhoField rhoField;
    typedef typename BasicTurbulenceModel::transportModel transportModel;


    //- Runtime type information
    TypeName("ML_RNG");

    // Constructors

        //- Construct from components
        ML_RNG
        (
            const alphaField& alpha,
            const rhoField& rho,
            const volVectorField& U,
            const surfaceScalarField& alphaRhoPhi,
            const surfaceScalarField& phi,
            const transportModel& transport,
            const word& propertiesName = turbulenceModel::propertiesName,
            const word& type = typeName,
            int omp_num_threads_ = 4
        );


    //- Destructor - 
    virtual ~ML_RNG()
    {
    }


    // Member Functions

        //- Read RASProperties dictionary
        virtual bool read();

        //- Return the effective diffusivity for nuTilda
        tmp<volScalarField> DnuTildaEff() const;

        //- Return the turbulence kinetic energy
        virtual tmp<volScalarField> k() const;

        //- Return the turbulence kinetic energy dissipation rate
        virtual tmp<volScalarField> epsilon() const;

        //- Solve the turbulence equations and correct the turbulence viscosity
        virtual void correct();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace RASModels
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#ifdef NoRepository
    #include "ML_RNG.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
