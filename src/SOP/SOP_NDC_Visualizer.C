
#include "SOP_NDC_Visualizer.h"

#include <SOP/SOP_Guide.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Error.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>

#include <iostream>

namespace mushyfruit{

static PRM_Name PRMcameraPath("camPath", "Path to Camera");
static PRM_Default camPathDefault(0, "/obj/cam1");

PRM_Template SOP_NDC_Visualizer::myTemplateList[] = {
    PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1,
                 &PRMcameraPath, &camPathDefault, 0, 0, 0,
                 &PRM_SpareData::objCameraPath),
    PRM_Template(PRM_STRING, 1, &PRMgroupName, 0,
                 &SOP_Node::pointGroupMenu, 0, 0, 
                 SOP_Node::getGroupSelectButton(GA_GROUP_POINT)),
    PRM_Template()
};

OP_Node *
SOP_NDC_Visualizer::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_NDC_Visualizer(net, name, op);
}

SOP_NDC_Visualizer::SOP_NDC_Visualizer(OP_Network *net, const char *name, OP_Operator *op)
    : SOP_Node(net, name, op)
{
    mySopFlags.setManagesDataIDs(true);
    mySopFlags.setNeedGuide1(true);
}

SOP_NDC_Visualizer::~SOP_NDC_Visualizer() {}

bool SOP_NDC_Visualizer::updateParmsFlags(){
    fpreal t = CHgetEvalTime();
    bool changed = SOP_Node::updateParmsFlags();
    return changed;
}

OP_ERROR
SOP_NDC_Visualizer::cookInputGroups(OP_Context &context, int alone)
{
    // The SOP_Node::cookInputPointGroups() provides a good default
    // implementation for just handling a point selection.
    return cookInputPointGroups(
        context, // This is needed for cooking the group parameter, and cooking the input if alone.
        myGroup, // The group (or NULL) is written to myGroup if not alone.
        alone,   // This is true iff called outside of cookMySop to update handles.
                 // true means the group will be for the input geometry.
                 // false means the group will be for gdp (the working/output geometry).
        true,    // (default) true means to set the selection to the group if not alone and the highlight flag is on.
        1,       // (default) Parameter index of the group field
        -1,      // (default) Parameter index of the group type field (-1 since there isn't one)
        true,    // (default) true means that a pointer to an existing group is okay; false means group is always new.
        false,   // (default) false means new groups should be unordered; true means new groups should be ordered.
        true,    // (default) true means that all new groups should be detached, so not owned by the detail;
                 //           false means that new point and primitive groups on gdp will be owned by gdp.
        0        // (default) Index of the input whose geometry the group will be made for if alone.
    );
}

OP_ERROR
SOP_NDC_Visualizer::cookMySop(OP_Context &context) {

    fpreal t = context.getTime();
    UT_String path;
    CAMPATH(path, t);
    
    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();

    duplicatePointSource(0, context);

    if (path.length() == 0) {
        addError(SOP_MESSAGE, "Path to camera is empty.");
        return error();
    }

    OBJ_Camera *camera_node;
    OBJ_Node *obj_node = OPgetDirector()->findOBJNode(path);
    if (!obj_node) {
        addError(SOP_MESSAGE, "Camera can't be located.");
        return error();
    }

    if (cookInputGroups(context) >= UT_ERROR_ABORT)
        return error();

    camera_node = obj_node->castToOBJCamera();
    addExtraInput(camera_node, OP_INTEREST_DATA);

    GA_Offset ptoff;
    GA_FOR_ALL_GROUP_PTOFF(gdp, myGroup, ptoff)
    {
        UT_Vector3 p = gdp->getPos3(ptoff);
        UT_Vector3 ndc_pos = translateToNDC(context, camera_node, p);
        gdp->setPos3(ptoff, ndc_pos);
    }

    // Get the selected point
    // Transform it to camera perspective
    // Display it within the guide as a representation of NDC space.

    if (!myGroup || !myGroup->isEmpty())
        gdp->getP()->bumpDataId();

    return error();
}

UT_Vector3 SOP_NDC_Visualizer::translateToNDC(OP_Context &context, OBJ_Camera *camera, UT_Vector3 pos)
{
    std::cout << pos << std::endl;
    //UT_Matrix4 deprecated
    // Use UT_Matrix4D -> fpreal64

    fpreal now = context.getTime();

    // Grab all the parm vals for the NDC
    const fpreal resx = camera->evalFloat("res", 0, now);
    const fpreal resy = camera->evalFloat("res", 1, now);
    const fpreal focal = camera->evalFloat("focal", 0, now);
    const fpreal aperture = camera->evalFloat("aperture", 0, now);
    const fpreal aspect = camera->evalFloat("aspect", 0, now);
    const fpreal near = camera->evalFloat("near", 0, now);
    const fpreal far = camera->evalFloat("far", 0, now);
    const fpreal par = camera->evalFloat("aspect", 0, now);

    // Aperture as base of triangle
    // Focal Length / Aperture = Scales the x/y coords impacting the zoom
    // These relate to how "zoomed" in the image appears.
    
    UT_Vector4 point_h = UT_Vector4(pos[0], pos[1], pos[2], 1);
    std::cout << point_h << std::endl;

    fpreal f = focal / aperture;
    fpreal aspect_ratio = resy / (resx * aspect);
    fpreal depth_scale = (far + near) / (far - near);
    fpreal depth_offset = (2.0 * far * near) / (far - near);

    UT_Matrix4D projectionMatrix = UT_Matrix4D(
        f,   0.0,            0.0,          0.0,
        0.0, f/aspect_ratio, 0.0,          0.0,
        0.0, 0.0,            depth_scale,  -1,
        0.0, 0.0,            depth_offset, 0.0
    );
    

    UT_Matrix4D worldTransform;
    camera->getWorldTransform(worldTransform, context);
    UT_Vector4 P_object = point_h * worldTransform;
    UT_Vector4 ndc = P_object * projectionMatrix;
    UT_Vector3 pndc = UT_Vector3(ndc[0] / ndc[3], ndc[1]/ndc[3], ndc[2]/ndc[3]);
    UT_Vector3 p_screen = pndc + UT_Vector3(0.5, 0.5, 0);
    p_screen *= 0.5;

    std::cout << "After" << std::endl;
    std::cout << p_screen << std::endl;
    return p_screen;
}

OP_ERROR
SOP_NDC_Visualizer::cookMyGuide1(OP_Context &context)
{

    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();

    myGuide1->clearAndDestroy();

    UT_Vector3RArray pos {
        UT_Vector3(1, 1, 1),    // up_right_far
        UT_Vector3(-1, 1, 1),   // up_left_far
        UT_Vector3(-1, -1, 1),  // down_left_far
        UT_Vector3(1, -1, 1),   // down_right_far
        UT_Vector3(1, 1, -1),   // up_right_near
        UT_Vector3(-1, 1, -1),  // up_left_near
        UT_Vector3(-1, -1, -1), // down_left_near
        UT_Vector3(1, -1, -1)   // down_right_near
    };

    GEO_PrimPoly *poly_front = GEO_PrimPoly::build(myGuide1, 4, GU_POLY_CLOSED);
    GEO_PrimPoly *poly_back = GEO_PrimPoly::build(myGuide1, 4, GU_POLY_CLOSED);

    // Set positions for front face
    for (int i=0; i < 4; i++) {
        GA_Offset ptoff = poly_front->getPointOffset(i);
        myGuide1->setPos3(ptoff, pos[i]);
    }

    // Set positions for back face
    for (int i=0; i < 4; i++) {
        GA_Offset ptoff = poly_back->getPointOffset(i);
        myGuide1->setPos3(ptoff, pos[i+4]);
    }

    // Connect up the side faces.
    for (int i=0; i < 4; i++) {
        GEO_PrimPoly *poly_side = GEO_PrimPoly::build(myGuide1, 4, GU_POLY_CLOSED);
        poly_side->setPointOffset(0, poly_front->getPointOffset(i));
        poly_side->setPointOffset(1, poly_front->getPointOffset((i+1)%4));
        poly_side->setPointOffset(2, poly_back->getPointOffset((i+1)%4));
        poly_side->setPointOffset(3, poly_back->getPointOffset(i));
    }

    myGuide1->drawXRay(true);

    return error();
}

} //namespace mushy

void newSopOperator(OP_OperatorTable *table)
{
    OP_Operator *op = new OP_Operator(
        "mushyfruit_ndc_visualizer",
        "NDC Visualizer",
        mushyfruit::SOP_NDC_Visualizer::myConstructor,
        mushyfruit::SOP_NDC_Visualizer::myTemplateList,
        1,
        1,
        0, OP_FLAG_GENERATOR
        );
    op->setOpTabSubMenuPath("mushyfruit");
    table->addOperator(op);
}