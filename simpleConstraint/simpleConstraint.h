#include <maya/MPxConstraint.h>
#include <maya/MPxConstraintCommand.h>
#include <maya/MDataBlock.h>
#include <maya/MDGModifier.h>
#include <maya/MDagPath.h>
#include <maya/MTypeId.h> 
#include <maya/MPlugArray.h>
#include <maya/MPlug.h>



class SimpleConstraint : public MPxConstraint
{
public:
    static MTypeId id;

    static MObject target;
    static MObject weight;
    static MObject targetTranslate;
	static MObject offset;

	//Input Attributes of constraint from target object
	static MObject targetRotatePivot;
	static MObject targetRotateTranslate;
	static MObject targetParentMatrix;

	// The input attribute connected from the constrained object
	static MObject constraintRotatePivot;
	static MObject constraintRotateTranslate;
	static MObject constraintParentInverseMatrix;

    // The output translation attribute, connected to the constrained object
    static MObject constraintTranslate;

    static void* creator();
    static MStatus initialize();

    virtual const MObject targetAttribute() const { return target; }
    virtual const MObject weightAttribute() const { return weight; }
	
    virtual void getOutputAttributes( MObjectArray& );
	virtual const MObject passiveOutputAttribute() const;

    virtual MStatus compute( const MPlug&, MDataBlock& );
};

class SimpleConstraintCommand : public MPxConstraintCommand
{
public:   
	
	static void* creator();
	
	//The following two functions needs to be implemented to support 
	//"-maintainOffset" and "-offset" flags 
	virtual bool supportsOffset() const { return true; }
	virtual const MObject& offsetAttribute() const { return SimpleConstraint::offset; }

    virtual MTypeId constraintTypeId() const { return SimpleConstraint::id; }

    virtual const MObject& objectAttribute() const { return MPxTransform::translate; }

    virtual const MObject& constraintOutputAttribute() const { return SimpleConstraint::constraintTranslate; }
    virtual const MObject& constraintTargetAttribute() const { return SimpleConstraint::target; }
    virtual const MObject& constraintTargetWeightAttribute() const { return SimpleConstraint::weight; }

    virtual MStatus connectTarget( MDagPath&, int );
    virtual MStatus connectObjectAndConstraint( MDGModifier& );
};
