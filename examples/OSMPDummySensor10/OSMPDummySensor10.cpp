/*
 * PMSF FMU Framework for FMI 1.0 Co-Simulation FMUs
 *
 * (C) 2016 -- 2017 PMSF IT Consulting Pierre R. Mai
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "OSMPDummySensor10.h"

/*
 * Debug Breaks
 *
 * If you define DEBUGBREAKS the DLL will automatically break
 * into an attached Debugger on all major computation functions.
 * Note that the DLL is likely to break all environments if no
 * Debugger is actually attached when the breaks are triggered.
 */
#ifndef NDEBUG
#ifdef DEBUGBREAKS
#include <intrin.h>
#define DEBUGBREAK() __debugbreak()
#else
#define DEBUGBREAK()
#endif
#else
#define DEBUGBREAK()
#endif

#include <iostream>
#include <string>
#include <algorithm>
#include <cstdint>

using namespace std;

#ifdef PRIVATE_LOG_PATH
ofstream COSMPDummySensor10::private_log_file;
#endif

/*
 * ProtocolBuffer Accessors
 */

void* decode_integer_to_pointer(fmiInteger hi,fmiInteger lo)
{
#if PTRDIFF_MAX == INT64_MAX
    union addrconv {
        struct {
            int lo;
            int hi;
        } base;
        unsigned long long address;
    } myaddr;
    myaddr.base.lo=lo;
    myaddr.base.hi=hi;
    return reinterpret_cast<void*>(myaddr.address);
#elif PTRDIFF_MAX == INT32_MAX
    return reinterpret_cast<void*>(lo);
#else
#error "Cannot determine 32bit or 64bit environment!"
#endif
}

void encode_pointer_to_integer(const void* ptr,fmiInteger& hi,fmiInteger& lo)
{
#if PTRDIFF_MAX == INT64_MAX
    union addrconv {
        struct {
            int lo;
            int hi;
        } base;
        unsigned long long address;
    } myaddr;
    myaddr.address=reinterpret_cast<unsigned long long>(ptr);
    hi=myaddr.base.hi;
    lo=myaddr.base.lo;
#elif PTRDIFF_MAX == INT32_MAX
    hi=0;
    lo=reinterpret_cast<int>(ptr);
#else
#error "Cannot determine 32bit or 64bit environment!"
#endif
}

bool COSMPDummySensor10::get_fmi_sensor_data_in(osi::SensorData& data)
{
    if (integer_vars[FMI_INTEGER_SENSORDATA_IN_SIZE_IDX] > 0) {
        void* buffer = decode_integer_to_pointer(integer_vars[FMI_INTEGER_SENSORDATA_IN_BASEHI_IDX],integer_vars[FMI_INTEGER_SENSORDATA_IN_BASELO_IDX]);
        normal_log("OSMP","Got %08X %08X, reading from %p ...",integer_vars[FMI_INTEGER_SENSORDATA_IN_BASEHI_IDX],integer_vars[FMI_INTEGER_SENSORDATA_IN_BASELO_IDX],buffer);
        data.ParseFromArray(buffer,integer_vars[FMI_INTEGER_SENSORDATA_IN_SIZE_IDX]);
        return true;
    } else {
        return false;
    }
}

void COSMPDummySensor10::set_fmi_sensor_data_out(const osi::SensorData& data)
{
    data.SerializeToString(&currentBuffer);
    encode_pointer_to_integer(currentBuffer.data(),integer_vars[FMI_INTEGER_SENSORDATA_OUT_BASEHI_IDX],integer_vars[FMI_INTEGER_SENSORDATA_OUT_BASELO_IDX]);
    integer_vars[FMI_INTEGER_SENSORDATA_OUT_SIZE_IDX]=(fmiInteger)currentBuffer.length();
    normal_log("OSMP","Providing %08X %08X, writing from %p ...",integer_vars[FMI_INTEGER_SENSORDATA_OUT_BASEHI_IDX],integer_vars[FMI_INTEGER_SENSORDATA_OUT_BASELO_IDX],currentBuffer.data());
    swap(currentBuffer,lastBuffer);
}

void COSMPDummySensor10::reset_fmi_sensor_data_out()
{
    integer_vars[FMI_INTEGER_SENSORDATA_OUT_SIZE_IDX]=0;
    integer_vars[FMI_INTEGER_SENSORDATA_OUT_BASEHI_IDX]=0;
    integer_vars[FMI_INTEGER_SENSORDATA_OUT_BASELO_IDX]=0;
}

/*
 * Actual Core Content
 */

fmiStatus COSMPDummySensor10::doInit()
{
    DEBUGBREAK();

    /* Booleans */
    for (int i = 0; i<FMI_BOOLEAN_VARS; i++)
        boolean_vars[i] = fmiFalse;

    /* Integers */
    for (int i = 0; i<FMI_INTEGER_VARS; i++)
        integer_vars[i] = 0;

    /* Reals */
    for (int i = 0; i<FMI_REAL_VARS; i++)
        real_vars[i] = 0.0;

    /* Strings */
    for (int i = 0; i<FMI_STRING_VARS; i++)
        string_vars[i] = "";

    return fmiOK;
}

fmiStatus COSMPDummySensor10::doStart(fmiReal startTime, fmiBoolean stopTimeDefined, fmiReal stopTime)
{
    DEBUGBREAK();
    last_time = startTime;
    return fmiOK;
}

void rotatePoint(double x, double y, double z,double yaw,double pitch,double roll,double &rx,double &ry,double &rz)
{
    double matrix[3][3];
    double cos_yaw = cos(yaw);
    double cos_pitch = cos(pitch);
    double cos_roll = cos(roll);
    double sin_yaw = sin(yaw);
    double sin_pitch = sin(pitch);
    double sin_roll = sin(roll);

    matrix[0][0] = cos_yaw*cos_pitch;  matrix[0][1]=cos_yaw*sin_pitch*sin_roll - sin_yaw*cos_roll; matrix[0][2]=cos_yaw*sin_pitch*cos_roll + sin_yaw*sin_roll;
    matrix[1][0] = sin_yaw*cos_pitch;  matrix[1][1]=sin_yaw*sin_pitch*sin_roll + cos_yaw*cos_roll; matrix[1][2]=sin_yaw*sin_pitch*cos_roll - cos_yaw*sin_roll;
    matrix[2][0] = -sin_pitch;         matrix[2][1]=cos_pitch*sin_roll;                            matrix[2][2]=cos_pitch*cos_roll;

    rx = matrix[0][0] * x + matrix[0][1] * y + matrix[0][2] * z;
    ry = matrix[1][0] * x + matrix[1][1] * y + matrix[1][2] * z;
    rz = matrix[2][0] * x + matrix[2][1] * y + matrix[2][2] * z;
}

fmiStatus COSMPDummySensor10::doCalc(fmiReal currentCommunicationPoint, fmiReal communicationStepSize, fmiBoolean newStep)
{
    DEBUGBREAK();
    osi::SensorData currentIn,currentOut;
    double time = currentCommunicationPoint+communicationStepSize;
    if (fmi_source()) {
        /* We act as GroundTruth Source, so ignore inputs */
        static double y_offsets[10] = { 3.0, 3.0, 3.0, 0.5, 0, -0.5, -3.0, -3.0, -3.0, -3.0 };
        static double x_offsets[10] = { 0.0, 40.0, 100.0, 100.0, 0.0, 150.0, 5.0, 45.0, 85.0, 125.0 };
        static double x_speeds[10] = { 29.0, 30.0, 31.0, 25.0, 26.0, 28.0, 20.0, 22.0, 22.5, 23.0 };
        currentOut.Clear();
        currentOut.mutable_ego_vehicle_id()->set_value(4);
        osi::SensorDataGroundTruth *currentSDGT = currentOut.mutable_ground_truth();
        osi::GroundTruth *currentGT = currentSDGT->mutable_global_ground_truth();
        currentOut.mutable_timestamp()->set_seconds((long long int)floor(time));
        currentOut.mutable_timestamp()->set_nanos((int)((time - floor(time))*1000000000.0));
        currentGT->mutable_timestamp()->set_seconds((long long int)floor(time));
        currentGT->mutable_timestamp()->set_nanos((int)((time - floor(time))*1000000000.0));
        for (unsigned int i=0;i<10;i++) {
            osi::Vehicle *veh = currentGT->add_vehicle();
            veh->set_type(osi::Vehicle_Type_TYPE_CAR);
            veh->mutable_id()->set_value(i);
            veh->set_ego_vehicle(i==4);
            veh->mutable_light_state()->set_brake_light_state(osi::Vehicle_LightState_BrakeLightState_BRAKE_LIGHT_STATE_OFF);
            veh->mutable_base()->mutable_dimension()->set_height(1.5);
            veh->mutable_base()->mutable_dimension()->set_width(2.0);
            veh->mutable_base()->mutable_dimension()->set_length(5.0);
            veh->mutable_base()->mutable_position()->set_x(x_offsets[i]+time*x_speeds[i]);
            veh->mutable_base()->mutable_position()->set_y(y_offsets[i]+sin(time/x_speeds[i])*0.25);
            veh->mutable_base()->mutable_position()->set_z(0.0);
            veh->mutable_base()->mutable_velocity()->set_x(x_speeds[i]);
            veh->mutable_base()->mutable_velocity()->set_y(cos(time/x_speeds[i])*0.25/x_speeds[i]);
            veh->mutable_base()->mutable_velocity()->set_z(0.0);
            veh->mutable_base()->mutable_acceleration()->set_x(0.0);
            veh->mutable_base()->mutable_acceleration()->set_y(-sin(time/x_speeds[i])*0.25/(x_speeds[i]*x_speeds[i]));
            veh->mutable_base()->mutable_acceleration()->set_z(0.0);
            veh->mutable_base()->mutable_orientation()->set_pitch(0.0);
            veh->mutable_base()->mutable_orientation()->set_roll(0.0);
            veh->mutable_base()->mutable_orientation()->set_yaw(0.0);
            veh->mutable_base()->mutable_orientation_rate()->set_pitch(0.0);
            veh->mutable_base()->mutable_orientation_rate()->set_roll(0.0);
            veh->mutable_base()->mutable_orientation_rate()->set_yaw(0.0);
            normal_log("OSI","GT: Adding Vehicle %d[%d] Absolute Position: %f,%f,%f Velocity (%f,%f,%f)",i,veh->id().value(),veh->base().position().x(),veh->base().position().y(),veh->base().position().z(),veh->base().velocity().x(),veh->base().velocity().y(),veh->base().velocity().z());
        }
        set_fmi_sensor_data_out(currentOut);
        set_fmi_valid(true);
        set_fmi_count(currentOut.object_size());
    } else if (get_fmi_sensor_data_in(currentIn)) {
        double ego_x=0, ego_y=0, ego_z=0;
        osi::Identifier ego_id = currentIn.ego_vehicle_id();
        normal_log("OSI","Looking for EgoVehicle with ID: %d",ego_id.value());
        for_each(currentIn.ground_truth().global_ground_truth().vehicle().begin(),currentIn.ground_truth().global_ground_truth().vehicle().end(),
            [this, ego_id, &ego_x, &ego_y, &ego_z](const osi::Vehicle& veh) {
                normal_log("OSI","Vehicle with ID %d is EgoVehicle: %d",veh.id().value(), veh.ego_vehicle() ? 1:0);
                if ((veh.id().value() == ego_id.value()) || (veh.ego_vehicle())) {
                    normal_log("OSI","Found EgoVehicle with ID: %d",veh.id().value());
                    ego_x = veh.base().position().x();
                    ego_y = veh.base().position().y();
                    ego_z = veh.base().position().z();
                }
            });
        normal_log("OSI","Current Ego Position: %f,%f,%f", ego_x, ego_y, ego_z);

        /* Clear Output */
        currentOut.Clear();
        /* Copy Everything */
        currentOut.MergeFrom(currentIn);
        /* Adjust Timestamps and Ids */
        currentOut.mutable_timestamp()->set_seconds((long long int)floor(time));
        currentOut.mutable_timestamp()->set_nanos((int)((time - floor(time))*1000000000.0));
        currentOut.mutable_object()->Clear();

        int i=0;
        for_each(currentIn.ground_truth().global_ground_truth().vehicle().begin(),currentIn.ground_truth().global_ground_truth().vehicle().end(),
            [this,&i,&currentOut,ego_id,ego_x,ego_y,ego_z](const osi::Vehicle& veh) {
                if (veh.id().value() != ego_id.value()) {
                    // NOTE: We currently do not take sensor mounting position into account,
                    // i.e. sensor-relative coordinates are relative to center of bounding box
                    // of ego vehicle currently.
                    double trans_x = veh.base().position().x()-ego_x;
                    double trans_y = veh.base().position().y()-ego_y;
                    double trans_z = veh.base().position().z()-ego_z;
                    double rel_x,rel_y,rel_z;
                    rotatePoint(trans_x,trans_y,trans_z,veh.base().orientation().yaw(),veh.base().orientation().pitch(),veh.base().orientation().roll(),rel_x,rel_y,rel_z);
                    double distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
                    if ((distance <= 150.0) && (rel_x/distance > 0.866025)) {
                        osi::DetectedObject *obj = currentOut.mutable_object()->Add();
                        obj->mutable_tracking_id()->set_value(i);
                        obj->mutable_object()->mutable_position()->set_x(veh.base().position().x());
                        obj->mutable_object()->mutable_position()->set_y(veh.base().position().y());
                        obj->mutable_object()->mutable_position()->set_z(veh.base().position().z());
                        obj->mutable_object()->mutable_dimension()->set_length(veh.base().dimension().length());
                        obj->mutable_object()->mutable_dimension()->set_width(veh.base().dimension().width());
                        obj->mutable_object()->mutable_dimension()->set_height(veh.base().dimension().height());
                        obj->set_existence_probability(cos((distance-75.0)/75.0));
                        normal_log("OSI","Output Vehicle %d[%d] Probability %f Relative Position: %f,%f,%f (%f,%f,%f)",i,veh.id().value(),obj->existence_probability(),rel_x,rel_y,rel_z,obj->object().position().x(),obj->object().position().y(),obj->object().position().z());
                        i++;
                    } else {
                        normal_log("OSI","Ignoring Vehicle %d[%d] Outside Sensor Scope Relative Position: %f,%f,%f (%f,%f,%f)",i,veh.id().value(),veh.base().position().x()-ego_x,veh.base().position().y()-ego_y,veh.base().position().z()-ego_z,veh.base().position().x(),veh.base().position().y(),veh.base().position().z());
                    }
                }
                else
                {
                    normal_log("OSI","Ignoring EGO Vehicle %d[%d] Relative Position: %f,%f,%f (%f,%f,%f)",i,veh.id().value(),veh.base().position().x()-ego_x,veh.base().position().y()-ego_y,veh.base().position().z()-ego_z,veh.base().position().x(),veh.base().position().y(),veh.base().position().z());
                }
            });
        normal_log("OSI","Mapped %d vehicles to output", i);
        /* Serialize */
        set_fmi_sensor_data_out(currentOut);
        set_fmi_valid(true);
        set_fmi_count(currentOut.object_size());
    } else {
        /* We have no valid input, so no valid output */
        reset_fmi_sensor_data_out();
        set_fmi_valid(false);
        set_fmi_count(0);
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::doTerm()
{
    DEBUGBREAK();
    return fmiOK;
}

void COSMPDummySensor10::doFree()
{
    DEBUGBREAK();
}

/*
 * Generic C++ Wrapper Code
 */

COSMPDummySensor10::COSMPDummySensor10(fmiString theinstanceName, fmiString thefmuGUID, fmiString thefmuLocation, fmiString themimeType, fmiReal thetimeout, fmiBoolean thevisible, fmiBoolean theinteractive, fmiCallbackFunctions thefunctions, fmiBoolean theloggingOn)
    : instanceName(theinstanceName),
    fmuGUID(thefmuGUID),
    fmuLocation(thefmuLocation),
    mimeType(themimeType),
    timeout(thetimeout),
    visible(!!thevisible),
    interactive(!!theinteractive),
    functions(thefunctions),
    loggingOn(!!theloggingOn),
    last_time(0.0)
{

}

COSMPDummySensor10::~COSMPDummySensor10()
{

}


fmiStatus COSMPDummySensor10::SetDebugLogging(fmiBoolean theloggingOn)
{
    fmi_verbose_log("fmiSetDebugLogging(%s)", theloggingOn ? "true" : "false");
    loggingOn = theloggingOn ? true : false;
    return fmiOK;
}

fmiComponent COSMPDummySensor10::Instantiate(fmiString instanceName, fmiString fmuGUID, fmiString fmuLocation, fmiString mimeType, fmiReal timeout, fmiBoolean visible, fmiBoolean interactive, fmiCallbackFunctions functions, fmiBoolean loggingOn)
{
    COSMPDummySensor10* myc = new COSMPDummySensor10(instanceName,fmuGUID,fmuLocation,mimeType,timeout,visible,interactive,functions,loggingOn);
    if (myc == NULL) {
        fmi_verbose_log_global("fmiInstantiate(\"%s\",\"%s\",\"%s\",\"%s\",%f,%d,%d,\"%s\",%d) = NULL (alloc failure)",
            (instanceName != NULL) ? instanceName : "<NULL>",
            (fmuGUID != NULL) ? fmuGUID : "<NULL>",
            (fmuLocation != NULL) ? fmuLocation : "<NULL>",
            (mimeType != NULL) ? mimeType : "<NULL>",
            timeout, visible, interactive, "FUNCTIONS", loggingOn);
        return NULL;
    }

    if (myc->doInit() != fmiOK) {
        fmi_verbose_log_global("fmiInstantiate(\"%s\",\"%s\",\"%s\",\"%s\",%f,%d,%d,\"%s\",%d) = NULL (doInit failure)",
            (instanceName != NULL) ? instanceName : "<NULL>",
            (fmuGUID != NULL) ? fmuGUID : "<NULL>",
            (fmuLocation != NULL) ? fmuLocation : "<NULL>",
            (mimeType != NULL) ? mimeType : "<NULL>",
            timeout, visible, interactive, "FUNCTIONS", loggingOn);
        delete myc;
        return NULL;
    }
    else {
        fmi_verbose_log_global("fmiInstantiate(\"%s\",\"%s\",\"%s\",\"%s\",%f,%d,%d,\"%s\",%d) = %p",
            (instanceName != NULL) ? instanceName : "<NULL>",
            (fmuGUID != NULL) ? fmuGUID : "<NULL>",
            (fmuLocation != NULL) ? fmuLocation : "<NULL>",
            (mimeType != NULL) ? mimeType : "<NULL>",
            timeout, visible, interactive, "FUNCTIONS", loggingOn, myc);
        return (fmiComponent)myc;
    }
}

fmiStatus COSMPDummySensor10::InitializeSlave(fmiReal startTime, fmiBoolean stopTimeDefined, fmiReal stopTime)
{
    fmi_verbose_log("fmiInitializeSlave(%g,%d,%g)", startTime, stopTimeDefined, stopTime);
    return doStart(startTime, stopTimeDefined, stopTime);
}

fmiStatus COSMPDummySensor10::DoStep(fmiReal currentCommunicationPoint, fmiReal communicationStepSize, fmiBoolean newStep)
{
    fmi_verbose_log("fmiDoStep(%g,%g,%d)", currentCommunicationPoint, communicationStepSize, newStep);
    return doCalc(currentCommunicationPoint, communicationStepSize, newStep);
}

fmiStatus COSMPDummySensor10::Terminate()
{
    fmi_verbose_log("fmiTerminate()");
    return doTerm();
}

fmiStatus COSMPDummySensor10::Reset()
{
    fmi_verbose_log("fmiReset()");

    doFree();
    return doInit();
}

void COSMPDummySensor10::FreeInstance()
{
    fmi_verbose_log("fmiFreeInstance()");
    doFree();
}

fmiStatus COSMPDummySensor10::GetReal(const fmiValueReference vr[], size_t nvr, fmiReal value[])
{
    fmi_verbose_log("fmiGetReal(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_REAL_VARS)
            value[i] = real_vars[vr[i]];
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::GetInteger(const fmiValueReference vr[], size_t nvr, fmiInteger value[])
{
    fmi_verbose_log("fmiGetInteger(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_INTEGER_VARS)
            value[i] = integer_vars[vr[i]];
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::GetBoolean(const fmiValueReference vr[], size_t nvr, fmiBoolean value[])
{
    fmi_verbose_log("fmiGetBoolean(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_BOOLEAN_VARS)
            value[i] = boolean_vars[vr[i]];
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::GetString(const fmiValueReference vr[], size_t nvr, fmiString value[])
{
    fmi_verbose_log("fmiGetString(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_STRING_VARS)
            value[i] = string_vars[vr[i]].c_str();
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::SetReal(const fmiValueReference vr[], size_t nvr, const fmiReal value[])
{
    fmi_verbose_log("fmiSetReal(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_REAL_VARS)
            real_vars[vr[i]] = value[i];
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::SetInteger(const fmiValueReference vr[], size_t nvr, const fmiInteger value[])
{
    fmi_verbose_log("fmiSetInteger(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_INTEGER_VARS)
            integer_vars[vr[i]] = value[i];
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::SetBoolean(const fmiValueReference vr[], size_t nvr, const fmiBoolean value[])
{
    fmi_verbose_log("fmiSetBoolean(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_BOOLEAN_VARS)
            boolean_vars[vr[i]] = value[i];
        else
            return fmiError;
    }
    return fmiOK;
}

fmiStatus COSMPDummySensor10::SetString(const fmiValueReference vr[], size_t nvr, const fmiString value[])
{
    fmi_verbose_log("fmiSetString(...)");
    for (size_t i = 0; i<nvr; i++) {
        if (vr[i]<FMI_STRING_VARS)
            string_vars[vr[i]] = value[i];
        else
            return fmiError;
    }
    return fmiOK;
}

/*
 * FMI 1.0 Co-Simulation Interface API
 */

extern "C" {

    DllExport const char* fmiGetTypesPlatform()
    {
        return fmiPlatform;
    }

    DllExport const char* fmiGetVersion()
    {
        return fmiVersion;
    }

    DllExport fmiStatus fmiSetDebugLogging(fmiComponent c, fmiBoolean loggingOn)
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->SetDebugLogging(loggingOn);
    }

    /*
    * Functions for Co-Simulation
    */
    DllExport fmiComponent fmiInstantiateSlave(fmiString instanceName,
        fmiString fmuGUID,
        fmiString fmuLocation,
        fmiString mimeType,
        fmiReal timeout,
        fmiBoolean visible,
        fmiBoolean interactive,
        fmiCallbackFunctions functions,
        fmiBoolean loggingOn)
    {
        return COSMPDummySensor10::Instantiate(instanceName, fmuGUID, fmuLocation, mimeType, timeout, visible, interactive, functions, loggingOn);
    }

    DllExport fmiStatus fmiInitializeSlave(fmiComponent c,
        fmiReal startTime,
        fmiBoolean stopTimeDefined,
        fmiReal stopTime)
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->InitializeSlave(startTime, stopTimeDefined, stopTime);
    }

    DllExport fmiStatus fmiDoStep(fmiComponent c,
        fmiReal currentCommunicationPoint,
        fmiReal communicationStepSize,
        fmiBoolean newStep)
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->DoStep(currentCommunicationPoint, communicationStepSize, newStep);
    }

    DllExport fmiStatus fmiTerminateSlave(fmiComponent c)
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->Terminate();
    }

    DllExport fmiStatus fmiResetSlave(fmiComponent c)
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->Reset();
    }

    DllExport void fmiFreeSlaveInstance(fmiComponent c)
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        myc->FreeInstance();
        delete myc;
    }

    /*
     * Data Exchange Functions
     */
    DllExport fmiStatus fmiGetReal(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiReal value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->GetReal(vr, nvr, value);
    }

    DllExport fmiStatus fmiGetInteger(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiInteger value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->GetInteger(vr, nvr, value);
    }

    DllExport fmiStatus fmiGetBoolean(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiBoolean value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->GetBoolean(vr, nvr, value);
    }

    DllExport fmiStatus fmiGetString(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiString value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->GetString(vr, nvr, value);
    }

    DllExport fmiStatus fmiSetReal(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiReal value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->SetReal(vr, nvr, value);
    }

    DllExport fmiStatus fmiSetInteger(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiInteger value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->SetInteger(vr, nvr, value);
    }

    DllExport fmiStatus fmiSetBoolean(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiBoolean value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->SetBoolean(vr, nvr, value);
    }

    DllExport fmiStatus fmiSetString(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiString value[])
    {
        COSMPDummySensor10* myc = (COSMPDummySensor10*)c;
        return myc->SetString(vr, nvr, value);
    }

    /*
     * Unsupported Features (FMUState, Derivatives, Async DoStep, Status Enquiries)
     */
    DllExport fmiStatus fmiSetRealInputDerivatives(fmiComponent c,
        const  fmiValueReference vr[],
        size_t nvr,
        const  fmiInteger order[],
        const  fmiReal value[])
    {
        return fmiError;
    }

    DllExport fmiStatus fmiGetRealOutputDerivatives(fmiComponent c,
        const   fmiValueReference vr[],
        size_t  nvr,
        const   fmiInteger order[],
        fmiReal value[])
    {
        return fmiError;
    }

    DllExport fmiStatus fmiCancelStep(fmiComponent c)
    {
        return fmiOK;
    }

    DllExport fmiStatus fmiGetStatus(fmiComponent c, const fmiStatusKind s, fmiStatus* value)
    {
        return fmiDiscard;
    }

    DllExport fmiStatus fmiGetRealStatus(fmiComponent c, const fmiStatusKind s, fmiReal* value)
    {
        return fmiDiscard;
    }

    DllExport fmiStatus fmiGetIntegerStatus(fmiComponent c, const fmiStatusKind s, fmiInteger* value)
    {
        return fmiDiscard;
    }

    DllExport fmiStatus fmiGetBooleanStatus(fmiComponent c, const fmiStatusKind s, fmiBoolean* value)
    {
        return fmiDiscard;
    }

    DllExport fmiStatus fmiGetStringStatus(fmiComponent c, const fmiStatusKind s, fmiString* value)
    {
        return fmiDiscard;
    }

}