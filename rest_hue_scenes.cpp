/*
 * Handle Hue-specific Dynamic Scenes.
 */

#include <optional>
#include <math.h>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

// Constants for 'timed_effect duration'
#define RESOLUTION_01s_BASE 0xFC
#define RESOLUTION_05s_BASE 0xCC
#define RESOLUTION_15s_BASE 0xA5
#define RESOLUTION_01m_BASE 0x79
#define RESOLUTION_05m_BASE 0x4A

#define RESOLUTION_01s (1 * 10)         // 01s.
#define RESOLUTION_05s (5 * 10)         // 05s.
#define RESOLUTION_15s (15 * 10)        // 15s.
#define RESOLUTION_01m (1 * 60 * 10)    // 01min.
#define RESOLUTION_05m (5 * 60 * 100)   // 05min.

#define RESOLUTION_01s_LIMIT (60 * 10)          // 01min.
#define RESOLUTION_05s_LIMIT (5 * 60 * 10)      // 05min.
#define RESOLUTION_15s_LIMIT (15 * 60 * 10)     // 15min.
#define RESOLUTION_01m_LIMIT (60 * 60 * 10)     // 60min.
#define RESOLUTION_05m_LIMIT (6 * 60 * 60 * 10) // 06hrs.

/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleHueScenesApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("hue-scenes"))
    {
        return REQ_NOT_HANDLED;
    }

    // PUT, PATCH /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/dynamic-state
    else if ((req.path.size() == 8) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH")  && (req.path[5] == "scenes") && (req.path[7] == "dynamic-state"))
    {
        return modifyHueDynamicScene(req, rsp);
    }
    // PUT /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/recall
    else if ((req.path.size() == 8) && (req.hdr.method() == "PUT")  && (req.path[5] == "scenes") && (req.path[7] == "recall"))
    {
        return recallHueDynamicScene(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/lights/<light_id>/state
    else if ((req.path.size() == 10) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH")  && (req.path[5] == "scenes") && (req.path[7] == "lights"))
    {
        return modifyHueScene(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/dynamic-state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifyHueDynamicScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    const QString &gid = req.path[4];
    const QString &sid = req.path[6];
    Scene *scene = nullptr;
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    if (req.sock)
    {
        userActivity();
    }

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/hue-scenes/groups/%1/scenes/%2/dynamic-state").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/hue-scenes/groups/%1/scenes/%2/dynamic-state").arg(gid).arg(sid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/dynamic-state").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists
    uint8_t sceneId = 0;
    ok = false;
    if (sid == QLatin1String("next") || sid == QLatin1String("prev"))
    {
        ResourceItem *item = group->item(RActionScene);
        DBG_Assert(item != 0);
        uint lastSceneId = 0;
        if (item && !item->toString().isEmpty())
        {
            lastSceneId = item->toString().toUInt(&ok);
        }

        int idx = -1;
        std::vector<quint8> scenes; // available scenes

        for (const Scene &s : group->scenes)
        {
            if (s.state != Scene::StateNormal)
            {
                continue;
            }

            if (lastSceneId == s.id)
            {
                idx = scenes.size(); // remember current index
            }
            scenes.emplace_back(s.id);
        }

        if (scenes.size() == 1)
        {
            ok = true;
            sceneId = scenes[0];
        }
        else if (scenes.size() > 1)
        {
            ok = true;
            if (idx == -1) // not found
            {
                idx = 0; // use first
            }
            else if (sid[0] == 'p') // prev
            {
                if (idx > 0)  { idx--; }
                else          { idx = scenes.size() - 1; } // jump to last scene
            }
            else // next
            {
                if (idx < int(scenes.size() - 1)) { idx++; }
                else  { idx = 0; } // jump to first scene
            }
            DBG_Assert(idx >= 0 && idx < int(scenes.size()));
            sceneId = scenes[idx];
        }
        // else ok == false
    }
    else
    {
        sceneId = sid.toUInt(&ok);
    }

    scene = ok ? group->getScene(sceneId) : nullptr;

    if (!scene || (scene->state != Scene::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/dynamic-state").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    QList<QString> validatedParameters;
    if (!validateHueDynamicScenePalette(rsp, scene, map, validatedParameters))
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // Populate the DynamicSceneState
    if (scene->dynamicState().has_value())
    {
        // !!!: Understand if this leaks memory
        scene->setDynamicState(std::nullopt);
    }

    if (!map.isEmpty())
    {
        scene->setDynamicState(scene->jsonToDynamics(Json::serialize(map)));
    }

    updateGroupEtag(group);

    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/recall
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::recallHueDynamicScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    const QString &gid = req.path[4];
    const QString &sid = req.path[6];
    Scene *scene = nullptr;
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    if (req.sock)
    {
        userActivity();
    }

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/hue-scenes/groups/%1/scenes/%2/recall").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/hue-scenes/groups/%1/scenes/%2/recall").arg(gid).arg(sid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/recall").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists
    uint8_t sceneId = 0;
    ok = false;
    if (sid == QLatin1String("next") || sid == QLatin1String("prev"))
    {
        ResourceItem *item = group->item(RActionScene);
        DBG_Assert(item != 0);
        uint lastSceneId = 0;
        if (item && !item->toString().isEmpty())
        {
            lastSceneId = item->toString().toUInt(&ok);
        }

        int idx = -1;
        std::vector<quint8> scenes; // available scenes

        for (const Scene &s : group->scenes)
        {
            if (s.state != Scene::StateNormal)
            {
                continue;
            }

            if (lastSceneId == s.id)
            {
                idx = scenes.size(); // remember current index
            }
            scenes.emplace_back(s.id);
        }

        if (scenes.size() == 1)
        {
            ok = true;
            sceneId = scenes[0];
        }
        else if (scenes.size() > 1)
        {
            ok = true;
            if (idx == -1) // not found
            {
                idx = 0; // use first
            }
            else if (sid[0] == 'p') // prev
            {
                if (idx > 0)  { idx--; }
                else          { idx = scenes.size() - 1; } // jump to last scene
            }
            else // next
            {
                if (idx < int(scenes.size() - 1)) { idx++; }
                else  { idx = 0; } // jump to first scene
            }
            DBG_Assert(idx >= 0 && idx < int(scenes.size()));
            sceneId = scenes[idx];
        }
        // else ok == false
    }
    else
    {
        sceneId = sid.toUInt(&ok);
    }

    scene = ok ? group->getScene(sceneId) : nullptr;

    if (!scene || (scene->state != Scene::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/recall").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    if (!callScene(group, sceneId))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2/recall").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    // If no dynamic state definition was provided, but there's one stored
    // for this scene, try recalling that.
    if (map.isEmpty() && scene->dynamicState().has_value())
    {
        QVariant var = Json::parse(scene->dynamicStateToString(), ok);

        if (ok)
        {
            map = var.toMap();
        }
    }

    QList<QString> validatedParameters;
    if (!validateHueDynamicScenePalette(rsp, scene, map, validatedParameters))
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    TaskItem taskRef;
    taskRef.req.setDstEndpoint(0xFF);
    taskRef.req.setDstAddressMode(deCONZ::ApsGroupAddress);
    taskRef.req.dstAddress().setGroup(group->address());
    taskRef.req.setSrcEndpoint(getSrcEndpoint(0, taskRef.req));

    if (!addTaskHueDynamicSceneRecall(taskRef, group->address(), scene->id, map))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2/recall").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    {
        const QString scid = QString::number(sceneId);
        ResourceItem *item = group->item(RActionScene);
        if (item && item->toString() != scid)
        {
            item->setValue(scid);
            updateGroupEtag(group);
            Event e(RGroups, RActionScene, group->id(), item);
            enqueueEvent(e);
        }
    }

    // TODO: Verify that the group's and lights' states update after the recall
    //       This call is meant to check the state of the lights have changed to match the
    //       recalled scene. Apparently, IKEA lights tend to misbehave. This might not be
    //       needed with Philips Hue lights that support effects.
    //recallSceneCheckGroupChanges(this, group, scene);

    updateEtag(gwConfigEtag);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    processTasks();

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/lights/<light_id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifyHueScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QString gid = req.path[4];
    QString sid = req.path[6];
    QString lid = req.path[8];
    Group *group = getGroupForId(gid);
    Scene scene;
    LightNode *light = getLightNodeForId(lid);
    LightState *lightState;
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /groups/%1, not available").arg(gid)));
        return REQ_READY_SEND;
    }

    if (!light || (light->state() == LightNode::StateDeleted) || !light->isAvailable())
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /lights/%1, not available").arg(lid)));
        return REQ_READY_SEND;
    }

    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    bool foundScene = false;
    bool foundLightState = false;

    for ( ;i != end; ++i)
    {
        if (QString::number(i->id) == sid && i->state != Scene::StateDeleted)
        {
            foundScene = true;
            scene = *i;

            std::vector<LightState>::iterator l = i->lights().begin();
            std::vector<LightState>::iterator lend = i->lights().end();

            for ( ;l != lend; ++l)
            {
                if (l->lid() == lid)
                {
                    foundLightState = true;
                    lightState = &(*l);

                    break;
                }
            }

            break;
        }
    }

    if (!foundScene)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /scenes/%1, not available").arg(sid)));
        return REQ_READY_SEND;
    }

    if (!foundLightState)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("Light %1 is not available in scene.").arg(lid)));
        return REQ_READY_SEND;
    }

    QList<QString> validatedParameters;
    if (!validateHueLightState(rsp, light, map, validatedParameters))
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // Update Scene's LightState
    lightState->setTransitionTime(map.contains("transitiontime") ? map["transitiontime"].toUInt() : lightState->transitionTime());
    lightState->setOn(map.contains("on") ? std::optional(map["on"].toBool()) : std::nullopt);
    lightState->setBri(map.contains("bri") ? std::optional(map["bri"].toUInt()) : std::nullopt);
    if (map.contains("xy"))
    {
        const QVariantList xy = map["xy"].toList();
        quint16 colorX = static_cast<quint16>(xy[0].toDouble() * 65535.0);
        quint16 colorY = static_cast<quint16>(xy[1].toDouble() * 65535.0);

        if (colorX > 65279) { colorX = 65279; }
        else if (colorX == 0) { colorX = 1; }

        if (colorY > 65279) { colorY = 65279; }
        else if (colorY == 0) { colorY = 1; }

        lightState->setColorMode("xy");
        lightState->setX(colorX);
        lightState->setY(colorY);
        lightState->setColorTemperature(std::nullopt);
    }
    else if (map.contains("ct"))
    {
        lightState->setColorMode("ct");
        lightState->setX(std::nullopt);
        lightState->setY(std::nullopt);
        lightState->setColorTemperature(map["ct"].toUInt());
    }
    else
    {
        lightState->setColorMode(std::nullopt);
        lightState->setX(std::nullopt);
        lightState->setY(std::nullopt);
        lightState->setColorTemperature(std::nullopt);
    }

    // Hue-specific attributes
    lightState->setEffect(map.contains("effect") ? std::optional(map["effect"].toString()) : std::nullopt);

    if (map.contains("effect_duration"))
    {
        // TODO: Consolidate with code in hue.cpp
        //      'streamHueManufacturerSpecificState()' does the same conversion
        //      and defines the same constants
        const uint ed = map["effect_duration"].toUInt();

        const uint resolutionBase = (ed == 0) ? 0 :
                                    (ed < RESOLUTION_01s_LIMIT) ? RESOLUTION_01s_BASE :
                                    (ed < RESOLUTION_05s_LIMIT) ? RESOLUTION_05s_BASE :
                                    (ed < RESOLUTION_15s_LIMIT) ? RESOLUTION_15s_BASE :
                                    (ed < RESOLUTION_01m_LIMIT) ? RESOLUTION_01m_BASE :
                                    (ed < RESOLUTION_05m_LIMIT) ? RESOLUTION_05m_BASE : 0;

        const uint resolution = (ed == 0) ? 1 :
                                (ed < RESOLUTION_01s_LIMIT) ? RESOLUTION_01s :
                                (ed < RESOLUTION_05s_LIMIT) ? RESOLUTION_05s :
                                (ed < RESOLUTION_15s_LIMIT) ? RESOLUTION_15s :
                                (ed < RESOLUTION_01m_LIMIT) ? RESOLUTION_01m :
                                (ed < RESOLUTION_05m_LIMIT) ? RESOLUTION_05m : 1;

        const uint8_t effectDuration = resolutionBase - (ed / resolution);
        lightState->setEffectDuration(effectDuration);
    }
    else
    {
        lightState->setEffectDuration(std::nullopt);
    }

    lightState->setEffectSpeed(map.contains("effect_speed") ? std::optional(map["effect_speed"].toDouble() * 254.0) : std::nullopt);


    TaskItem taskRef;
    taskRef.lightNode = getLightNodeForId(lid);
    taskRef.req.dstAddress() = taskRef.lightNode->address();
    taskRef.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    taskRef.req.setDstEndpoint(taskRef.lightNode->haEndpoint().endpoint());
    taskRef.req.setSrcEndpoint(getSrcEndpoint(taskRef.lightNode, taskRef.req));
    taskRef.req.setDstAddressMode(deCONZ::ApsExtAddress);

    if (!addTaskHueManufacturerSpecificAddScene(taskRef, group->address(), scene.id, map))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    updateGroupEtag(group);

    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}
