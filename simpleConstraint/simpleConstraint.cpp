#include "SimpleConstraint.h"
#include <maya/MFnTransform.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFileIO.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <maya/MFnPlugin.h>
#include <assert.h>
#include <vector>

MTypeId SimpleConstraint::id( 0x001078e2 );

// Compound attribute for the targets
MObject SimpleConstraint::target;

// Each target has the following child attributes: target weight and target translation value
MObject SimpleConstraint::weight;
MObject SimpleConstraint::targetTranslate;      

//Offset attribute for keeping offset when the constraint is active
MObject SimpleConstraint::offset;

//Input Attributes of constraint from target object
MObject SimpleConstraint::targetRotatePivot;
MObject SimpleConstraint::targetRotateTranslate;
MObject SimpleConstraint::targetParentMatrix;

// Input Attributes of constraint from constrained object 
MObject SimpleConstraint::constraintRotatePivot;
MObject SimpleConstraint::constraintRotateTranslate;
MObject SimpleConstraint::constraintParentInverseMatrix;

// Simple Attributes that affect the final position of the constrained object
MObject SimpleConstraint::constraintTranslate;



MStatus initializePlugin( MObject obj )
{
    MFnPlugin plugin( obj, "Autodesk", "1.0" );

    plugin.registerNode( "simpleConstraint",
                         SimpleConstraint::id,
                         SimpleConstraint::creator,
                         SimpleConstraint::initialize,
                         MPxNode::kConstraintNode );

    plugin.registerConstraintCommand("simpleConstraintCmd", SimpleConstraintCommand::creator);

    return MStatus::kSuccess;
}

MStatus uninitializePlugin( MObject obj )
{
    MFnPlugin plugin( obj );

    plugin.deregisterNode( SimpleConstraint::id );
    plugin.deregisterConstraintCommand( "simpleConstraintCmd" );

    return MStatus::kSuccess;
}

void* SimpleConstraint::creator()
{
    return new SimpleConstraint();
}

MStatus SimpleConstraint::initialize()
{
    MFnCompoundAttribute cAttr;
    MFnNumericAttribute nAttr;
    MFnMatrixAttribute mAttr;

    // Parameters for each target
	targetTranslate = nAttr.create("targetTranslate", "tt", MFnNumericData::k3Double);
    
	targetRotatePivot = nAttr.create("targetRotatePivot","trp",MFnNumericData::k3Double);
	targetRotateTranslate = nAttr.create("targetRotateTranslate","trt",MFnNumericData::k3Double);
	targetParentMatrix = mAttr.create("targetParentMatrix", "tpm");
	
	weight = nAttr.create( "weight", "wt", MFnNumericData::kDouble, 1.0 );

    // Compound target parameter
    target = cAttr.create( "target", "tar" );
    cAttr.setHidden( true );
    cAttr.addChild( targetTranslate );
	cAttr.addChild( targetRotatePivot);
	cAttr.addChild( targetRotateTranslate);
	cAttr.addChild( targetParentMatrix);
    cAttr.addChild( weight );
    cAttr.setArray( true );
    addAttribute( target );

	offset = nAttr.create("offset", "os",MFnNumericData::k3Double);
	addAttribute(offset);
	
	constraintRotatePivot = nAttr.create("constraintRotatePivot","crp", MFnNumericData::k3Double);
	addAttribute(constraintRotatePivot);

	constraintRotateTranslate = nAttr.create("constraintRotateTranslate","crt", MFnNumericData::k3Double);
	addAttribute(constraintRotateTranslate);

	constraintParentInverseMatrix = mAttr.create("constraintParentInverseMatrix", "cpim");
	addAttribute(constraintParentInverseMatrix);

	constraintTranslate = nAttr.create( "constraintTranslate", "ct", MFnNumericData::k3Double );
    addAttribute( constraintTranslate );

    // Setup the "attribute affects"
    attributeAffects( target, constraintTranslate );
	attributeAffects( offset, constraintTranslate);

	attributeAffects( constraintRotatePivot, constraintTranslate);
	attributeAffects( constraintRotateTranslate, constraintTranslate);
	attributeAffects( constraintParentInverseMatrix, constraintTranslate);

    return MStatus::kSuccess;
}

void SimpleConstraint::getOutputAttributes( MObjectArray& objects )
{
    objects.clear();
    objects.append( constraintTranslate );
}

const MObject SimpleConstraint::passiveOutputAttribute() const
{
	return constraintTranslate;
}

MStatus SimpleConstraint::compute( const MPlug& plug, MDataBlock& data )
{
	MStatus status;

    if( plug == constraintTranslate || plug.parent() == constraintTranslate )
    {
        // For this simple constraint we just get the translation from the first target
        MArrayDataHandle targetArray = data.inputArrayValue( target );
		
		//Calculate the weight for each target, if they are all zero, just set the constraintTranslate clean without doing anything
		double weightSum = 0.0;
		double individualweight = 0;
		for (int i = 0; i < targetArray.elementCount(); i++)
		{
			individualweight = targetArray.inputValue().child(weight).asDouble();
			weightSum += individualweight;
			targetArray.next();
		}
		if(weightSum == 0.0)
		{
			data.setClean(constraintTranslate);
			return MStatus::kUnknownParameter; 
		}

		//Reset weightSum 
		weightSum = 0.0;
		MMatrix tarparentMatrix;
		MMatrix objParentInverseMatrix;
		MPoint sumPos(0.0,0.0,0.0);

		//Reset the array handle so that it starts at the beginning of the array
		targetArray = data.inputArrayValue( target );
		for (int i = 0; i < targetArray.elementCount(); i++)
        {
			MDataHandle elementHandle = targetArray.inputValue();
			double3& tarTranslation = elementHandle.child( targetTranslate ).asDouble3();
			double3& tarRotatePivot = elementHandle.child( targetRotatePivot ).asDouble3();
			double3& tarRotateTranslate = elementHandle.child( targetRotateTranslate ).asDouble3();
			double curWeight = elementHandle.child( weight ).asDouble();

			MPoint tmpPoint(tarTranslation[0] + tarRotatePivot[0] + tarRotateTranslate[0],
							tarTranslation[1] + tarRotatePivot[1] + tarRotateTranslate[1],
							tarTranslation[2] + tarRotatePivot[2] + tarRotateTranslate[2]);

			tarparentMatrix = elementHandle.child(targetParentMatrix).asMatrix();
			tmpPoint = tmpPoint * tarparentMatrix;

			sumPos[0] += tmpPoint[0] * curWeight;
			sumPos[1] += tmpPoint[1] * curWeight;
			sumPos[2] += tmpPoint[2] * curWeight;

			weightSum += curWeight;
			targetArray.next();
		}
		
		weightSum = 1.0 / weightSum;
		sumPos[0] = sumPos[0] * weightSum;
		sumPos[1] = sumPos[1] * weightSum;
		sumPos[2] = sumPos[2] * weightSum;


		objParentInverseMatrix = data.inputValue(constraintParentInverseMatrix).asMatrix();
		sumPos  = sumPos * objParentInverseMatrix;


		double3& offsetVal = data.inputValue(offset,&status).asDouble3();
		sumPos[0] += offsetVal[0];
		sumPos[1] += offsetVal[1];
		sumPos[2] += offsetVal[2];


        //Retrieve constraint object rotatePivot and rotatePivotTranslate information
		double3& rpdata = data.inputValue(constraintRotatePivot).asDouble3();
		double3& rtdata = data.inputValue(constraintRotateTranslate).asDouble3();
		MPoint PivotPoint(rpdata[0] + rtdata[0], rpdata[1] + rtdata[1], rpdata[2] + rtdata[2]);
				
		MVector result;				
		result.x = sumPos[0] - PivotPoint[0];
		result.y = sumPos[1] - PivotPoint[1];
		result.z = sumPos[2] - PivotPoint[2];
				
		data.outputValue( constraintTranslate ).set(result.x, result.y, result.z);

		
        return MStatus::kSuccess;            
        }

	return MStatus::kUnknownParameter;
   
}

//SimpleConstraintCommand implementation

void* SimpleConstraintCommand::creator()
{
    return new SimpleConstraintCommand();
}


MStatus SimpleConstraintCommand::connectTarget( MDagPath& path, int index )
{
	MStatus status;
	// Connect target attributes to input attributes of constrain
	status = connectTargetAttribute(path, index, MPxTransform::rotatePivot,SimpleConstraint::targetRotatePivot,false);
	status = connectTargetAttribute(path, index, MPxTransform::rotatePivotTranslate,SimpleConstraint::targetRotateTranslate,false);
	status = connectTargetAttribute(path, index, MPxTransform::parentMatrix, SimpleConstraint::targetParentMatrix, false);
    status = connectTargetAttribute(path, index, MPxTransform::translate, SimpleConstraint::targetTranslate, false);
    return status;
}

MStatus SimpleConstraintCommand::connectObjectAndConstraint( MDGModifier& modifier )
{
	MStatus status;
	
	// Connect the attributes of constrained object to the constraint
	status = connectObjectAttribute(MPxTransform::rotatePivot, SimpleConstraint::constraintRotatePivot,true,false);
	status = connectObjectAttribute(MPxTransform::rotatePivotTranslate,SimpleConstraint::constraintRotateTranslate, true,false);
	status = connectObjectAttribute(MPxTransform::parentInverseMatrix,SimpleConstraint::constraintParentInverseMatrix, true, false);
    //
    // Connect the constraint output attribute to the constrained object
    //
    status = connectObjectAttribute( MPxTransform::translate,SimpleConstraint::constraintTranslate, false, false );


    return status;
}
