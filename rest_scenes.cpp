/*
 * Copyright (c) 2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <math.h>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleScenesApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("scenes"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/scenes
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllScenes(req, rsp);
    }

     return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/scenes
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllScenes(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    std::vector<Group>::const_iterator g = groups.begin();
    std::vector<Group>::const_iterator gend = groups.end();

    for (; g != gend; ++g)
    {
        // ignore deleted groups
        if (g->state() == Group::StateDeleted || g->state() == Group::StateDeleteFromDB)
        {
            continue;
        }

        if (g->address() != gwGroup0) // don't return special group 0
        {
            QVariantMap scenes;
            std::vector<Scene>::const_iterator s = g->scenes.begin();
            std::vector<Scene>::const_iterator send = g->scenes.end();

            for (; s != send; ++s)
            {
                if (s->state == Scene::StateNormal)
                {
                    QString sceneId = QString::number(s->id);
                    QVariantMap scene;

                    QVariantList lights;
                    QVariantMap states;
                    std::vector<LightState>::const_iterator l = s->lights().begin();
                    std::vector<LightState>::const_iterator lend = s->lights().end();
                    for (; l != lend; ++l)
                    {
                        lights.append(l->lid());

                        QVariantMap lstate;
                        lstate["id"] = l->lid();

                        if (l->on().has_value())
                        {
                            lstate["on"] = l->on().value();
                        }

                        if (l->bri().has_value())
                        {
                            lstate["bri"] = l->bri().value();
                        }

                        LightNode *lightNode = getLightNodeForId(l->lid());
                        if (lightNode && lightNode->hasColor() && l->colorMode().has_value())
                        {
                            if (l->colorMode().value() == QLatin1String("xy") && l->x().has_value() && l->y().has_value())
                            {
                                QVariantList xy;
                                double x = l->x().value();
                                double y = l->y().value();

                                if (x > 0xFEFF) x = 0xFEFF;
                                if (y > 0xFEFF) y = 0xFEFF;

                                xy.append(round(x / 6.5535) / 10000.0);
                                xy.append(round(y / 6.5535) / 10000.0);

                                lstate["xy"] = xy;
                            }
                            else if (l->colorMode().value() == QLatin1String("ct") && l->colorTemperature().has_value())
                            {
                                lstate["ct"] = (double)l->colorTemperature().value();
                            }
                            else if (l->colorMode().value() == QLatin1String("hs"))
                            {
                                lstate["hue"] = (double)l->enhancedHue();
                                lstate["sat"] = (double)l->saturation();
                            }

                            lstate["colormode"] = l->colorMode().value();
                        }
                        lstate["transitiontime"] = l->transitionTime();

                        states[l->lid()] = lstate;
                    }

                    scene["id"] = sceneId;
                    scene["lights"] = lights;
                    scene["name"] = s->name;
                    scene["states"] = states;
                    scene["transitiontime"] = s->transitiontime();

                    // Hue Dynamic States
                    if (s->dynamicState().has_value())
                    {
                        bool ok;
                        QVariant var = Json::parse(s->dynamicStateToString(), ok);

                        if (ok)
                        {
                            scene["dynamics"] = var.toMap();
                        }
                    }

                    scenes[sceneId] = scene;
                }
            }

            QVariantMap group;
            QVariantList lights;
            std::vector<LightNode>::const_iterator ln = nodes.begin();
            std::vector<LightNode>::const_iterator lnend = nodes.end();

            for (; ln != lnend; ++ln)
            {
                if (ln->state() == LightNode::StateDeleted)
                {
                    continue;
                }

                std::vector<GroupInfo>::const_iterator lng = ln->groups().begin();
                std::vector<GroupInfo>::const_iterator lngend = ln->groups().end();

                for (; lng != lngend; ++lng)
                {
                    if (lng->id == g->address())
                    {
                        if (lng->state == GroupInfo::StateInGroup)
                        {
                            lights.append(ln->id());
                        }
                        break;
                    }
                }
            }

            group["id"] = g->id();
            group["lights"] = lights;
            group["name"] = g->name();
            group["scenes"] = scenes;

            rsp.map[g->id()] = group;
        }
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    return REQ_READY_SEND;
}