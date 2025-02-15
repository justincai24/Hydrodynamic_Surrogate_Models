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

\*---------------------------------------------------------------------------*/
#include <omp.h>
#include "ML_KO.H"
#include "fvOptions.H"
#include "bound.H"
#include "wallDist.H"
#include <vector>
#include <math.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
namespace Foam
{
namespace RASModels
{

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::chi() const
{
    return nuTilda_/this->nu();
}


template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::fv1
(
    const volScalarField& chi
) const
{
    const volScalarField chi3(pow3(chi));
    return chi3/(chi3 + pow3(Cv1_));
}


template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::fv2
(
    const volScalarField& chi,
    const volScalarField& fv1
) const
{
    return 1.0 - chi/(1.0 + chi*fv1);
}


template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::Stilda
(
    const volScalarField& chi,
    const volScalarField& fv1
)   const
{
    volScalarField Omega(::sqrt(2.0)*mag(skew(fvc::grad(this->U_))));

    return
    (
        max
        (
            Omega
          + fv2(chi, fv1)*nuTilda_/sqr(kappa_*y_),
            Cs_*Omega
        )
    );
}


template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::fw
(
    const volScalarField& Stilda
)   const
{
    volScalarField r
    (
        min
        (
            nuTilda_
           /(
               max
               (
                   Stilda,
                   dimensionedScalar("SMALL", Stilda.dimensions(), SMALL)
               )
              *sqr(kappa_*y_)
            ),
            scalar(10.0)
        )
    );
    r.boundaryFieldRef() == 0.0;

    const volScalarField g(r + Cw2_*(pow6(r) - r));

    return g*pow((1.0 + pow6(Cw3_))/(pow6(g) + pow6(Cw3_)), 1.0/6.0);
}


template<class BasicTurbulenceModel>
void ML_KO<BasicTurbulenceModel>::correctNut
(
    const volScalarField& fv1
)
{
    this->nut_ = nuTilda_*fv1;
    this->nut_.correctBoundaryConditions();
    fv::options::New(this->mesh_).correct(this->nut_);

    BasicTurbulenceModel::correctNut();
}


template<class BasicTurbulenceModel>
void ML_KO<BasicTurbulenceModel>::correctNut()
{
    correctNut(fv1(this->chi()));
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class BasicTurbulenceModel>
ML_KO<BasicTurbulenceModel>::ML_KO
(
    const alphaField& alpha,
    const rhoField& rho,
    const volVectorField& U,
    const surfaceScalarField& alphaRhoPhi,
    const surfaceScalarField& phi,
    const transportModel& transport,
    const word& propertiesName,
    const word& type,
    int omp_num_threads_
)
:
    eddyViscosity<RASModel<BasicTurbulenceModel>>
    (
        type,
        alpha,
        rho,
        U,
        alphaRhoPhi,
        phi,
        transport,
        propertiesName
    ),

    sigmaNut_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "sigmaNut",
            this->coeffDict_,
            0.66666
        )
    ),
    kappa_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "kappa",
            this->coeffDict_,
            0.41
        )
    ),

    Cb1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cb1",
            this->coeffDict_,
            0.1355
        )
    ),
    Cb2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cb2",
            this->coeffDict_,
            0.622
        )
    ),
    Cw1_(Cb1_/sqr(kappa_) + (1.0 + Cb2_)/sigmaNut_),
    Cw2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cw2",
            this->coeffDict_,
            0.3
        )
    ),
    Cw3_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cw3",
            this->coeffDict_,
            2.0
        )
    ),
    Cv1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cv1",
            this->coeffDict_,
            7.1
        )
    ),
    Cs_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cs",
            this->coeffDict_,
            0.3
        )
    ),

    // Follow convention for graph operations - append '_ph' for placeholder, simply '_' for output, '_op' for an operation like backprop
    // Constructor loads a frozen *.pb graph from the path 
    graph_
    (
    	tf_utils::LoadGraph("./ML_KO.pb")
    ),

    // Defines an input operation - this must correspond to the name specified in python API
    input_ph_
    (
    	{TF_GraphOperationByName(graph_, "input_placeholder"), 0} // the operation would look like "input_placeholder:0" in model.inputs[0] (keras)

    ),
    // Defines an output operation - this must correspond to the name specified in python API
    output_
    (
    	{TF_GraphOperationByName(graph_, "output_value/BiasAdd"), 0} // the operation would look like "output_value/BiasAdd:0" in model.outputs[0] (keras)
    ),
    nuTilda_
    (
        IOobject
        (
            "nuTilda",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    umag_
    (
        IOobject
        (
            "umag",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    gradu_
    (
        IOobject
        (
            "gradu",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    gradv_
    (
        IOobject
        (
            "gradv",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    rot_
    (
        IOobject
        (
            "rot",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    vortz_
    (
        IOobject
        (
            "vortz",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    nut_ml_
    (
        IOobject
        (
            "nut_ml_",
            this->runTime_.timeName(),
            this->mesh_,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        this->mesh_
    ),
    y_(wallDist::New(this->mesh_).y()),
    // Filtering operations - simpleFilter
    MyFilter_
    (
        this->mesh_
    ),
    // filterPtr_
    // (
    //     LESfilter::New(this->mesh_, this->coeffDict())
    // ),
    // filter_
    // (
    //     filterPtr_()
    // ),
    omp_num_threads_(4)
    {
        if (type == typeName)
        {
            this->printCoeffs(type);
        }
    }

// * * * * * * * * * * * * * * * * Constructing filter  * * * * * * * * * * * * * * //
// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class BasicTurbulenceModel>
bool ML_KO<BasicTurbulenceModel>::read()
{
    if (eddyViscosity<RASModel<BasicTurbulenceModel>>::read())
    {
        sigmaNut_.readIfPresent(this->coeffDict());
        kappa_.readIfPresent(this->coeffDict());

        Cb1_.readIfPresent(this->coeffDict());
        Cb2_.readIfPresent(this->coeffDict());
        Cw1_ = Cb1_/sqr(kappa_) + (1.0 + Cb2_)/sigmaNut_;
        Cw2_.readIfPresent(this->coeffDict());
        Cw3_.readIfPresent(this->coeffDict());
        Cv1_.readIfPresent(this->coeffDict());
        Cs_.readIfPresent(this->coeffDict());

        return true;
    }
    else
    {
        return false;
    }
}

template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::DnuTildaEff() const
{
    return tmp<volScalarField>
    (
        new volScalarField("DnuTildaEff", (nuTilda_ + this->nu())/sigmaNut_)
    );
}


template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::k() const
{
    return tmp<volScalarField>
    (
        new volScalarField
        (
            IOobject
            (
                "k",
                this->runTime_.timeName(),
                this->mesh_
            ),
            this->mesh_,
            dimensionedScalar("0", dimensionSet(0, 2, -2, 0, 0), 0)
        )
    );
}


template<class BasicTurbulenceModel>
tmp<volScalarField> ML_KO<BasicTurbulenceModel>::epsilon() const
{
    WarningInFunction
        << "Turbulence kinetic energy dissipation rate not defined for "
        << "Spalart-Allmaras model. Returning zero field"
        << endl;

    return tmp<volScalarField>
    (
        new volScalarField
        (
            IOobject
            (
                "epsilon",
                this->runTime_.timeName(),
                this->mesh_
            ),
            this->mesh_,
            dimensionedScalar("0", dimensionSet(0, 2, -3, 0, 0), 0)
        )
    );
}

template<class BasicTurbulenceModel>
void ML_KO<BasicTurbulenceModel>::correct()
{
  if (!this->turbulence_)
  {
      return;
  }

  eddyViscosity<RASModel<BasicTurbulenceModel>>::correct();

  int num_inputs = 4;
  int num_outputs = 1;  
  double mean_array [num_inputs+num_outputs] = {3.974928417108912271e+01,-1.200867154544176252e+00,4.808239254796427969e-02,-4.134784774507140048e-02,1.263535215909553349e-03};
  double std_array [num_inputs+num_outputs] = {5.450719336407781057e+00,4.413283008198113144e+00,4.545372374067887383e-02,3.161890801185181510e-01,1.449872100441359926e-03};

  run_ml_graph(mean_array,std_array,num_inputs,num_outputs);

}

template<class BasicTurbulenceModel>
void ML_KO<BasicTurbulenceModel>::run_ml_graph(double* mean_array, double* std_array, int num_inputs, int num_outputs)
{
    omp_set_num_threads(omp_num_threads_);

    // Structure for tensors in ML
    int num_cells = this->mesh_.cells().size();

    // Velocity related
    volScalarField u_ = this->U_.component(vector::X);
    volScalarField v_ = this->U_.component(vector::Y);
    // Velocity magnitude
    umag_ = MyFilter_(sqrt(magSqr(this->U_)));
    // Velocity related
    gradu_ = MyFilter_(y_*y_*magSqr(fvc::grad(this->U_.component(vector::X))));
    gradv_ = MyFilter_(y_*y_*magSqr(fvc::grad(this->U_.component(vector::Y))));
    // // Pressure related
    volScalarField p_ = MyFilter_(this->db().objectRegistry::lookupObject<volScalarField>("p"));
    // Strain and rotation
    rot_ = MyFilter_(sqrt(2.0)*mag(skew(fvc::grad(this->U_))));
    // Vorticity
    volVectorField vort_ = fvc::curl(this->U_);
  	vortz_ = MyFilter_(vort_.component(vector::Z));
    // Datastructure for output
    nut_ml_ = this->nut_;
    // Local references
    const alphaField& alpha = this->alpha_;
    const rhoField& rho = this->rho_;
    const volScalarField chi(this->chi());
    const volScalarField fv1(this->fv1(chi));
    const volScalarField Stilda(this->Stilda(chi, fv1));

    volScalarField source_term = fvc::Sp(Cw1_*alpha*rho*fw(Stilda)*nuTilda_/sqr(y_), nuTilda_);
    
    // Some tensorflow pointer requirements
    TF_Status* status_ = TF_NewStatus();
    TF_SessionOptions* options_ = TF_NewSessionOptions();
    TF_Session* sess_ = TF_NewSession(graph_, options_, status_);
    
	float input_vals[num_cells][num_inputs];
	const std::vector<std::int64_t> input_dims = {num_cells, num_inputs};

    volScalarField cx_ = this->mesh_.C().component(vector::X);
    volScalarField cy_ = this->mesh_.C().component(vector::Y);

    // #pragma omp parallel for
    forAll(umag_.internalField(), id) // for boundary field use u_.boundaryField()
    {

        float i1 = (u_[id] - mean_array[0])/(std_array[0]);
        float i2 = (v_[id] - mean_array[1])/(std_array[1]);
        float i3 = (cy_[id] - mean_array[2])/(std_array[2]);
        float i4 = (cx_[id] - mean_array[3])/(std_array[3]);
        
        input_vals[id][0] = i1;
        input_vals[id][1] = i2;
        input_vals[id][2] = i3;    
        input_vals[id][3] = i4;
    }

    // Set up TF C API stuff
    TF_Tensor* output_tensor_ = nullptr;
    TF_Tensor* input_tensor_ = tf_utils::CreateTensor(TF_FLOAT,
                                          input_dims.data(), input_dims.size(),
                                          &input_vals, num_cells*num_inputs*sizeof(float));
    
    // Arrays of tensors
    TF_Tensor* input_tensors_[1] = {input_tensor_};
    TF_Tensor* output_tensors_[1] = {output_tensor_};
    // Arrays of operations
    TF_Output inputs[1] = {input_ph_};
    TF_Output outputs[1] = {output_};
    
    TF_SessionRun(sess_,
                nullptr, // Run options.
                inputs, input_tensors_, 1, // Input tensor ops, input tensor values, number of inputs.
                outputs, output_tensors_, 1, // Output tensor ops, output tensor values, number of outputs.
                nullptr, 0, // Target operations, number of targets.
                nullptr, // Run metadata.
                status_ // Output status.
                );

	const auto data = static_cast<float*>(TF_TensorData(output_tensors_[0]));
	for (int i = 0; i < num_cells; i++)
	{
		nut_ml_[i] = data[num_outputs*i]*std_array[num_inputs] + mean_array[num_inputs]; // Funnel changes back into OF - row major order	
	}

	tf_utils::DeleteTensor(input_tensor_);
	tf_utils::DeleteTensor(output_tensor_);
    TF_DeleteSessionOptions(options_);
    TF_DeleteStatus(status_);
    tf_utils::DeleteSession(sess_);

    nut_ml_ = MyFilter_(nut_ml_);

    forAll(nut_ml_.internalField(), id)
    {
        this->nut_[id] = max(nut_ml_[id],0.0);
    }

    this->nut_.correctBoundaryConditions();
    fv::options::New(this->mesh_).correct(this->nut_);
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace RASModels
} // End namespace Foam

// ************************************************************************* //
