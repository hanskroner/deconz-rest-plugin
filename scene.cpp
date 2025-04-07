/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */
#include <QStringBuilder>
#include <math.h>
#include "scene.h"

/*! Constructor.
 */
Scene::Scene() :
    state(StateNormal),
    externalMaster(false),
    groupAddress(0),
    id(0),
    m_transitiontime(0),
    m_dynamicState(std::nullopt)
{
}

/*! Returns the transitiontime of the scene.
 */
const uint16_t &Scene::transitiontime() const
{
    return m_transitiontime;
}

/*! Sets the transitiontime of the scene.
    \param transitiontime the transitiontime of the scene
 */
void Scene::setTransitiontime(const uint16_t &transitiontime)
{
    m_transitiontime = transitiontime;
}

/*! Returns the lights of the scene.
 */
std::vector<LightState> &Scene::lights()
{
    return m_lights;
}

/*! Returns the lights of the scene.
 */
const std::vector<LightState> &Scene::lights() const
{
    return m_lights;
}

/*! Sets the lights of the scene.
    \param lights the lights of the scene
 */
void Scene::setLights(const std::vector<LightState> &lights)
{
    m_lights = lights;
}

/*! Returns the dynamic scene of the scene - if any.
 */
const std::optional<DynamicSceneState> &Scene::dynamicState() const
{
    return m_dynamicState;
}

/*! Sets the dynamic scene of the scene - if any.
    \param dynamic the dynamic state of the scene
 */
void Scene::setDynamicState(const std::optional<DynamicSceneState> &dynamic)
{
    m_dynamicState = dynamic;
}

/*! Adds a light to the lights of the scene.
    \param light the light that should be added
 */
void Scene::addLightState(const LightState &light)
{
    m_lights.push_back(light);
}

/*! removes a light from the lights of the scene if present.
    \param lid the lightId that should be removed
    \return true if light was found and removed
 */
bool Scene::deleteLight(const QString &lid)
{
    std::vector<LightState>::const_iterator l = m_lights.begin();
    std::vector<LightState>::const_iterator lend = m_lights.end();
    int position = 0;
    for (; l != lend; ++l)
    {
        if (l->lid() == lid)
        {
            m_lights.erase(m_lights.begin() + position);
            // delete scene if it contains no lights
            if (m_lights.size() == 0)
            {
                state = Scene::StateDeleted;
            }
            return true;
        }
        position++;
    }
    return false;
}

/*! Returns light satte for given light id of the scene if present.
    \param lid the lightId
    \return the light state or 0 if not found
 */
LightState *Scene::getLightState(const QString &lid)
{
    std::vector<LightState>::iterator i = m_lights.begin();
    std::vector<LightState>::iterator end = m_lights.end();

    for (; i != end; ++i)
    {
        if (i->lid() == lid)
        {
           return &*i;
        }
    }
    return 0;
}

/*! Exports the dynamic state of the scene into JSONString.
 */
QString Scene::dynamicStateToString() const
{
    QVariantMap map;

    if (m_dynamicState->brightness().has_value())
    {
        map[QLatin1String("bri")] = (double)m_dynamicState->brightness().value();
    }

    if (m_dynamicState->colorTemperature().has_value())
    {
        map[QLatin1String("ct")] = (double)m_dynamicState->colorTemperature().value();
    }

    if (m_dynamicState->colorPalette().has_value())
    {
        QVariantList ls;
        std::vector<DynamicSceneColor>::const_iterator i = m_dynamicState->colorPalette().value().begin();
        std::vector<DynamicSceneColor>::const_iterator end = m_dynamicState->colorPalette().value().end();

        for (; i != end; ++i)
        {
            QVariantList color;
            color.append(i->x);
            color.append(i->y);
            // !!!: QVariantList.append() is overloaded and would flatten the list
            //      Use QVariantList.insert() instead.
            ls.insert(ls.size(), color);
        }

        map[QLatin1String("xy")] = ls;
    }

    map[QLatin1String("auto_dynamic")] = (bool)m_dynamicState->autoDynamic();
    map[QLatin1String("effect_speed")] = (double)m_dynamicState->effectSpeed();

    return Json::serialize(map);
}

/*! Imports the dynamic state of the scene from JSONString.
    \param json a JSON string that represents the state of a dynamic scene
 */
DynamicSceneState Scene::jsonToDynamics(const QString &json) const
{
    bool ok;
    QVariant var = Json::parse(json, ok);
    QVariantMap map = var.toMap();
    DynamicSceneState dynamicState;

    if (!ok)
    {
        return dynamicState;
    }

    // !!!: Not doing any validation

    if (map.contains("bri"))
    {
        dynamicState.setBrightness(map["bri"].toUInt());
    }

    if (map.contains("ct"))
    {
        dynamicState.setColorTemperature(map["ct"].toUInt());
    }

    if (map.contains("xy")) {
        std::vector<DynamicSceneColor> colorsVector;
        QVariantList colors = map["xy"].toList();

        // Palette Colors
        for (auto &color : colors)
        {
            QVariantList xy = color.toList();
            double x = xy[0].toDouble();
            double y = xy[1].toDouble();

            colorsVector.push_back(DynamicSceneColor {x, y});
        }

        dynamicState.setColorPalette(colorsVector);
    }

    if (map.contains("auto_dynamic"))
    {
        dynamicState.setAutoDynamic(map["auto_dynamic"].toBool());
    }

    if (map.contains("effect_speed"))
    {
        dynamicState.setEffectSpeed(map["effect_speed"].toDouble());
    }

    return dynamicState;
}

/*! Transfers lights of the scene into JSONString.
    \param lights vector<LightState>
 */
QString Scene::lightsToString(const std::vector<LightState> &lights)
{
    std::vector<LightState>::const_iterator i = lights.begin();
    std::vector<LightState>::const_iterator i_end = lights.end();
    QVariantList ls;

    for (; i != i_end; ++i)
    {
        QVariantMap map;
        map[QLatin1String("lid")] = i->lid();

        if (i->on().has_value())
        {
            map[QLatin1String("on")] = i->on().value();
        }

        if (i->bri().has_value())
        {
            map[QLatin1String("bri")] = (double)i->bri().value();
        }

        map[QLatin1String("tt")] = (double)i->transitionTime();

        if (i->colorMode().has_value())
        {
            map[QLatin1String("cm")] = i->colorMode().value();

            if (i->colorMode().value() != QLatin1String("none"))
            {
                if (i->x().has_value() && i->y().has_value())
                {
                    map[QLatin1String("x")] = (double)i->x().value();
                    map[QLatin1String("y")] = (double)i->y().value();
                }

                if (i->colorMode().value() == QLatin1String("hs"))
                {
                    map[QLatin1String("ehue")] = (double)i->enhancedHue();
                    map[QLatin1String("sat")] = (double)i->saturation();
                }
                else if (i->colorMode().value() == QLatin1String("ct") && i->colorTemperature().has_value())
                {
                    map[QLatin1String("ct")] = (double)i->colorTemperature().value();
                }

                map[QLatin1String("cl")] = i->colorloopActive();
                map[QLatin1String("clTime")] = (double)i->colorloopTime();
            }
        }

        // Hue-specific attributes

        if (i->effect().has_value())
        {
            map[QLatin1String("effect")] = i->effect().value();
        }

        if (i->effectDuration().has_value())
        {
            map[QLatin1String("effect_duration")] = (double)i->effectDuration().value();
        }

        if (i->effectSpeed().has_value())
        {
            map[QLatin1String("effect_speed")] = (double)i->effectSpeed().value();
        }

        ls.append(map);
    }

    return Json::serialize(ls);
}

std::vector<LightState> Scene::jsonToLights(const QString &json)
{
    bool ok;
    QVariantList var = Json::parse(json, ok).toList();
    QVariantMap map;
    std::vector<LightState> lights;

    QVariantList::const_iterator i = var.begin();
    QVariantList::const_iterator i_end = var.end();

    if (!ok)
    {
        return lights;
    }

    for (; i != i_end; ++i)
    {
        LightState state;
        // Clear out optionals
        state.setOn(std::nullopt);
        state.setBri(std::nullopt);
        state.setColorMode(std::nullopt);
        state.setX(std::nullopt);
        state.setY(std::nullopt);
        state.setColorTemperature(std::nullopt);

        map = i->toMap();
        state.setLightId(map[QLatin1String("lid")].toString());

        if (map.contains(QLatin1String("on")))
        {
            state.setOn(map[QLatin1String("on")].toBool());
        }
        if (map.contains(QLatin1String("bri")))
        {
            state.setBri(map[QLatin1String("bri")].toUInt());
        }

        state.setTransitionTime(map[QLatin1String("tt")].toUInt());

        if (map.contains(QLatin1String("x")) && map.contains(QLatin1String("y")))
        {
            state.setX(map[QLatin1String("x")].toUInt());
            state.setY(map[QLatin1String("y")].toUInt());

            if (!map.contains(QLatin1String("cm")))
            {
                state.setColorMode(QLatin1String("xy")); // backward compatibility
            }
        }

        if (map.contains(QLatin1String("cl")) && map.contains(QLatin1String("clTime")))
        {
            state.setColorloopActive(map[QLatin1String("cl")].toBool());
            state.setColorloopTime(map[QLatin1String("clTime")].toUInt());
        }

        if (map.contains(QLatin1String("cm")))
        {
            QString colorMode = map[QLatin1String("cm")].toString();
            if (!colorMode.isEmpty())
            {
                state.setColorMode(colorMode);
            }
        }

        if (state.colorMode() == QLatin1String("ct") && map.contains(QLatin1String("ct")))
        {
            quint16 ct = map[QLatin1String("ct")].toUInt(&ok);
            if (ok)
            {
                state.setColorTemperature(ct);
            }
        }
        else if (state.colorMode() == QLatin1String("hs") && map.contains(QLatin1String("ehue")) && map.contains(QLatin1String("sat")))
        {
            quint16 ehue = map[QLatin1String("ehue")].toUInt(&ok);
            if (ok)
            {
                quint16 sat = map[QLatin1String("sat")].toUInt(&ok);
                if (ok)
                {
                    state.setEnhancedHue(ehue);
                    state.setSaturation(sat);
                }
            }
        }

        // Hue-specific attributes
        if (map.contains(QLatin1String("effect")))
        {
            state.setEffect(map[QLatin1String("effect")].toString());
        }
        if (map.contains(QLatin1String("effect_duration")))
        {
            state.setEffectDuration(map[QLatin1String("effect_duration")].toUInt());
        }
        if (map.contains(QLatin1String("effect_speed")))
        {
            state.setEffectSpeed(map[QLatin1String("effect_speed")].toUInt());
        }

        lights.push_back(state);
    }

    return lights;
}


// LightState

/*! Constructor.
 */
LightState::LightState() :
    m_lid(""),
    m_on(false),
    m_needRead(false),
    m_bri(0),
    m_x(0),
    m_y(0),
    m_enhancedHue(0),
    m_saturation(0),
    m_colorloopActive(false),
    m_colorloopDirection(0),
    m_colorloopTime(0),
    m_colorMode(QLatin1String("none")),
    m_transitiontime(0),
    m_effect(std::nullopt),
    m_effectDuration(std::nullopt),
    m_effectSpeed(std::nullopt)
{
}

/*! Returns the id of the light of the scene.
 */
const QString &LightState::lid() const
{
    return m_lid;
}

/*! Sets the id of the light of the scene.
    \param state the rule state
 */
void LightState::setLightId(const QString &lid)
{
    m_lid = lid;
}

/*! Returns the on status of the light of the scene.
 */
const std::optional<bool> &LightState::on() const
{
    return m_on;
}

/*! Sets the on status of the light of the scene.
    \param on the on status of the light
 */
void LightState::setOn(const std::optional<bool> &on)
{
    m_on = on;
}

/*! Returns the brightness of the light of the scene.
 */
const std::optional<uint8_t> &LightState::bri() const
{
    return m_bri;
}

/*! Sets the brightness of the light of the scene.
    \param bri the brightness of the light
 */
void LightState::setBri(const std::optional<uint8_t> &bri)
{
    m_bri = bri;
}

/*! Returns the colorX value of the light of the scene.
 */
const std::optional<uint16_t> &LightState::x() const
{
    return m_x;
}

/*! Sets the colorX value of the light of the scene.
    \param x the colorX value of the light
 */
void LightState::setX(const std::optional<uint16_t> &x)
{
    m_x = x;
}

/*! Returns the colorY value of the light of the scene.
 */
const std::optional<uint16_t> &LightState::y() const
{
    return m_y;
}

/*! Sets the colorY value of the light of the scene.
    \param y the colorY value of the light
 */
void LightState::setY(const std::optional<uint16_t> &y)
{
    m_y = y;
}

/*! Returns the color temperature value of the light in the scene.
 */
const std::optional<uint16_t> &LightState::colorTemperature() const
{
    return m_colorTemperature;
}

/*! Sets the color temperature value of the light in the scene.
    \param colorTemperature the color temperature value of the light
 */
void LightState::setColorTemperature(const std::optional<uint16_t> &colorTemperature)
{
    m_colorTemperature = colorTemperature;
}

/*! Returns the enhancedHue value of the light of the scene.
 */
const uint16_t &LightState::enhancedHue() const
{
    return m_enhancedHue;
}

/*! Sets the enhancedHue value of the light of the scene.
    \param enhancedHue the enhancedHue value of the light
 */
void LightState::setEnhancedHue(const uint16_t &enhancedHue)
{
    m_enhancedHue = enhancedHue;
}

/*! Returns the saturation of the light of the scene.
 */
const uint8_t &LightState::saturation() const
{
    return m_saturation;
}

/*! Sets the saturation of the light of the scene.
    \param sat the saturation of the light
 */
void LightState::setSaturation(const uint8_t &sat)
{
    m_saturation = sat;
}

/*! Returns the colorloopActive status of the light of the scene.
 */
const bool &LightState::colorloopActive() const
{
    return m_colorloopActive;
}

/*! Sets the colorloopActive status of the light of the scene.
    \param active the colorloopActive status of the light
 */
void LightState::setColorloopActive(const bool &active)
{
    m_colorloopActive = active;
}

/*! Returns the colorloopDirection of the light of the scene.
 */
const uint8_t &LightState::colorloopDirection() const
{
    return m_colorloopDirection;
}

/*! Sets the colorloopDirection of the light of the scene.
    \param direction the colorloopDirection of the light
 */
void LightState::setColorloopDirection(const uint8_t &direction)
{
    m_colorloopDirection = direction;
}

/*! Returns the colorloopTime of the light of the scene.
 */
const uint8_t &LightState::colorloopTime() const
{
    return m_colorloopTime;
}

/*! Sets the colorloopTime of the light of the scene.
    \param time the colorloopTime of the light
 */
void LightState::setColorloopTime(const uint8_t &time)
{
    m_colorloopTime = time;
}

/*! Returns the color mode of the light in the scene.
 */
const std::optional<QString> &LightState::colorMode() const
{
    return m_colorMode;
}

/*! Sets the color mode of the light in the scene.
    \param colorMode the color mode of the light
 */
void LightState::setColorMode(const std::optional<QString> &colorMode)
{
    if (m_colorMode != colorMode)
    {
        m_colorMode = colorMode;
    }
}

/*! Returns the transitiontime of the scene.
 */
const uint16_t &LightState::transitionTime() const
{
    return m_transitiontime;
}

/*! Sets the transitiontime of the scene.
    \param transitiontime the transitiontime of the scene
 */
void LightState::setTransitionTime(uint16_t transitiontime)
{
    m_transitiontime = transitiontime;
}

// Hue-specific attributes


/*! Returns the effect of the scene.
 */
const std::optional<QString> &LightState::effect() const
{
    return m_effect;
}

/*! Sets the effect of the scene.
    \param effect the effect of the scene
 */
void LightState::setEffect(const std::optional<QString> &effect)
{
    m_effect = effect;
}

/*! Returns the effect duration of the scene.
 */
const std::optional<uint8_t> &LightState::effectDuration() const
{
    return m_effectDuration;
}

/*! Sets the effect duration of the scene.
    \param effectDuration the effect duration of the scene
 */
void LightState::setEffectDuration(const std::optional<uint8_t> &effectDuration)
{
    m_effectDuration = effectDuration;
}

/*! Returns the effect speed of the scene.
 */
const std::optional<uint8_t> &LightState::effectSpeed() const
{
    return m_effectSpeed;
}

/*! Sets the effect speed of the scene.
    \param effectSpeed the effect speed of the scene
 */
void LightState::setEffectSpeed(const std::optional<uint8_t> &effectSpeed)
{
    m_effectSpeed = effectSpeed;
}
/*! Sets need read flag.
    \param needRead - true if attribute should be queried by view scene command
 */
void LightState::setNeedRead(bool needRead)
{
    m_needRead = needRead;
}


// LightState

/*! Constructor.
 */
DynamicSceneState::DynamicSceneState() :
    m_brightness(std::nullopt),
    m_colorTemperature(std::nullopt),
    m_autoDynamic(false),
    m_effectSpeed(152) // 0.6
{
}

/*! Returns the 'bri' value for the dynamic scene of the scene - if any.
 */
const std::optional<uint8_t> &DynamicSceneState::brightness() const
{
    return m_brightness;
}

/*! Sets the 'bri' value for the dynamic scene of the scene - if any.
    \param brightness the brightness value for the dynamic state of the scene
 */
void DynamicSceneState::setBrightness(const std::optional<uint8_t> &brightness)
{
    m_brightness = brightness;
}

/*! Returns the 'ct' value for the dynamic scene of the scene - if any.
 */
const std::optional<uint16_t> &DynamicSceneState::colorTemperature() const
{
    return m_colorTemperature;
}

/*! Sets the 'ct' value for the dynamic scene of the scene - if any.
    \param colorTemperature the color temperature values for the dynamic state of the scene
 */
void DynamicSceneState::setColorTemperature(const std::optional<uint16_t> &colorTemperature)
{
    m_colorTemperature = colorTemperature;
}

/*! Return the collection of colors used in this dynamic scene.
 */
const std::optional<std::vector<DynamicSceneColor>> &DynamicSceneState::colorPalette() const
{
    return m_colorPalette;
}

/*! Sets the collection of colors used in this dynamic scene.
    \param colorPalette a collection of colors used in this dynamic scene
 */
void DynamicSceneState::setColorPalette(const std::optional<std::vector<DynamicSceneColor>> &colorPalette)
{
    m_colorPalette = colorPalette;
}

/*! Returns whether this dynamic scene should auto-play on recall.
 */
const bool &DynamicSceneState::autoDynamic() const
{
    return m_autoDynamic;
}

/*! Sets whether this dynamic scene should auto-play on recall.
    \param autoDynamic whether this dynamic scene should auto-play on recall
 */
void DynamicSceneState::setAutoDynamic(const bool &autoDynamic)
{
    m_autoDynamic = autoDynamic;
}

/*! Returns the effect speed for this dynamic scene.
 */
const double &DynamicSceneState::effectSpeed() const
{
    return m_effectSpeed;
}

/*! Returns the effect speed for this dynamic scene.
    \param effectSpeed the effect speed for this dynamic scene
 */
void DynamicSceneState::setEffectSpeed(const double &effectSpeed)
{
    m_effectSpeed = effectSpeed;
}