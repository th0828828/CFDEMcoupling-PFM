/*---------------------------------------------------------------------------*\
    CFDEMcoupling - Open Source CFD-DEM coupling

    CFDEMcoupling is part of the CFDEMproject
    www.cfdem.com
                                Christoph Goniva, christoph.goniva@cfdem.com
                                Copyright 2009-2012 JKU Linz
                                Copyright 2012-     DCS Computing GmbH, Linz
-------------------------------------------------------------------------------
License
    This file is part of CFDEMcoupling.

    CFDEMcoupling is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 3 of the License, or (at your
    option) any later version.

    CFDEMcoupling is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with CFDEMcoupling; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Description
    This code is designed to realize coupled CFD-DEM simulations using LIGGGHTS
    and OpenFOAM(R). Note: this code is not part of OpenFOAM(R) (see DISCLAIMER).
\*---------------------------------------------------------------------------*/

#include "error.H"

#include "SchillerNaumannDrag.H"
#include "addToRunTimeSelectionTable.H"

//#include <mpi.h>

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

defineTypeNameAndDebug(SchillerNaumannDrag, 0);

addToRunTimeSelectionTable
(
    forceModel,
    SchillerNaumannDrag,
    dictionary
);


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

// Construct from components
SchillerNaumannDrag::SchillerNaumannDrag
(
    const dictionary& dict,
    cfdemCloud& sm
)
:
    forceModel(dict,sm),
    propsDict_(dict.subDict(typeName + "Props")),
    verbose_(propsDict_.found("verbose")),
    velFieldName_(propsDict_.lookup("velFieldName")),
    U_(sm.mesh().lookupObject<volVectorField> (velFieldName_)),
    scaleDia_(1.),
    scaleDrag_(1.)
{
    //Append the field names to be probed
    particleCloud_.probeM().initialize(typeName, typeName+".logDat");
    particleCloud_.probeM().vectorFields_.append("dragForce"); //first entry must the be the force
    particleCloud_.probeM().vectorFields_.append("Urel");        //other are debug
    particleCloud_.probeM().scalarFields_.append("Rep");          //other are debug
    particleCloud_.probeM().scalarFields_.append("Cd");                 //other are debug
    particleCloud_.probeM().writeHeader();

    // init force sub model
    setForceSubModels(propsDict_);

    // define switches which can be read from dict
    forceSubM(0).setSwitchesList(SW_TREAT_FORCE_EXPLICIT,true); // activate treatExplicit switch

    // read those switches defined above, if provided in dict
    forceSubM(0).readSwitches();

    particleCloud_.checkCG(true);

    if (propsDict_.found("scale"))
        scaleDia_ = scalar(readScalar(propsDict_.lookup("scale")));
    if (propsDict_.found("scaleDrag"))
        scaleDrag_ = scalar(readScalar(propsDict_.lookup("scaleDrag")));
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

SchillerNaumannDrag::~SchillerNaumannDrag()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void SchillerNaumannDrag::setForce() const
{
    if (scaleDia_ > 1.0)
    {
        Info << "SchillerNaumann using scale = " << scaleDia_ << endl;
    }
    else if (particleCloud_.cg() > 1.0)
    {
        scaleDia_ = particleCloud_.cg();
        Info << "SchillerNaumann using scale from liggghts cg = " << scaleDia_ << endl;
    }

    #include "setupProbeModel.H"

    const volScalarField& nufField = forceSubM(0).nuField();
    const volScalarField& rhoField = forceSubM(0).rhoField();

    for(int index = 0;index <  particleCloud_.numberOfParticles(); index++)
    {
        //if(mask[index][0])
        //{
            vector drag(0,0,0);
            label cellI = particleCloud_.cellIDs()[index][0];

            if (cellI > -1) // particle Found
            {
                //NP note: one could add pointInterpolated values instead of cell centered
                vector Us = particleCloud_.velocity(index);
                vector Ur = U_[cellI]-Us;
                scalar ds = 2*particleCloud_.radius(index);
                scalar ds_scaled = ds/scaleDia_;
                scalar nuf = nufField[cellI];
                scalar rho = rhoField[cellI];
                scalar voidfraction = particleCloud_.voidfraction(index);
                scalar magUr = mag(Ur);
                scalar Rep = 0;
                scalar Cd = 0;

                if (magUr > 0)
                {
                   // calc particle Re Nr
                    Rep = ds_scaled*magUr/nuf;

                    // calc fluid drag Coeff
                    Cd = max(0.44,24.0/Rep*(1.0+0.15*pow(Rep,0.687)));

                    // calc particle's drag
                    drag = 0.125*Cd*rho*M_PI*ds*ds*scaleDia_*magUr*Ur*scaleDrag_;

                    if (modelType_=="B")
                        drag /= voidfraction;
                }

                if(verbose_ && index >=100 && index <102)
                {
                    Pout << "cellI = " << cellI << endl;
                    Pout << "index = " << index << endl;
                    Pout << "Ufluid = " << U_[cellI] << endl;
                    Pout << "Us = " << Us << endl;
                    Pout << "Ur = " << Ur << endl;
                    Pout << "ds = " << ds << endl;
                    Pout << "ds/scale = " << ds_scaled << endl;
                    Pout << "rho = " << rho << endl;
                    Pout << "nuf = " << nuf << endl;
                    Pout << "voidfraction = " << voidfraction << endl;
                    Pout << "Rep = " << Rep << endl;
                    Pout << "Cd = " << Cd << endl;
                    Pout << "drag = " << drag << endl;
                }

                //Set value fields and write the probe
                if(probeIt_)
                {
                    #include "setupProbeModelfields.H"
                    vValues.append(drag);           //first entry must the be the force
                    vValues.append(Ur);
                    sValues.append(Rep);
                    sValues.append(Cd);
                    particleCloud_.probeM().writeProbe(index, sValues, vValues);
                }
            }

            // write particle based data to global array
            forceSubM(0).partToArray(index,drag,vector::zero);
        //}
    }

}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
