// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include <QString>

inline QString inputObjectName(const ballistic::ValidationIssue &issue) {
    using ballistic::InputField;
    const char *name = "";
    switch (issue.field) {
    case InputField::Payload: name = "payload"; break;
    case InputField::Mass: name = "mass"; break;
    case InputField::Fuel: name = "fuel"; break;
    case InputField::Thrust: name = "thrust"; break;
    case InputField::ExhaustVelocity: name = "exhaust"; break;
    case InputField::VerticalTime: name = "verticalTime"; break;
    case InputField::TurnTime: name = "turnTime"; break;
    case InputField::TurnDegrees: name = "turnAngle"; break;
    case InputField::MaxStep: name = "maxStep"; break;
    case InputField::TargetAltitude: name = "targetAltitude"; break;
    case InputField::EngineCutoffTime: name = "engineCutoffTime"; break;
    default: break;
    }
    const auto result = QString::fromLatin1(name);
    return issue.stage < 0 ? result : result + QString::number(issue.stage + 1);
}
