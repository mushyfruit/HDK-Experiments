#ifndef __SOP_BoxGenerator_h__
#define __SOP_BoxGenerator_h__

#include <SOP/SOP_Node.h>
#include <OBJ/OBJ_Camera.h>

namespace mushyfruit {
class SOP_NDC_Visualizer : public SOP_Node
{
public:

    // PRM_Template List
    static PRM_Template myTemplateList[];

    //Constructor to pass to Node Template
    static OP_Node *myConstructor(OP_Network*, const char *, OP_Operator *);

    // Update disable/hidden states of parameters based on others.
    virtual bool updateParmsFlags();

    virtual OP_ERROR cookInputGroups(OP_Context &context, int alone=0) override;
    
protected:
    //Define the class constructor
    SOP_NDC_Visualizer(OP_Network *net, const char *name, OP_Operator *op);

    //Define the class destructor
    virtual ~SOP_NDC_Visualizer();

    virtual OP_ERROR cookMySop(OP_Context &context);
    virtual OP_ERROR cookMyGuide1(OP_Context &context);

    UT_Vector3 translateToNDC(OP_Context &context, OBJ_Camera *camera, UT_Vector3 pos);

private:
    // Convention for these is to use CAPS
    void CAMPATH(UT_String &str, fpreal t) { evalString(str, "camPath", 0, t); }

    /// This is the group of geometry to be manipulated by this SOP and cooked
    /// by the method "cookInputGroups".
    const GA_PointGroup *myGroup;

};
} //namespace mushy_fruit

#endif