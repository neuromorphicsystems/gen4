#pragma once

#include <QtCore/QByteArray>

namespace assets {

    const auto opensans_regular = QByteArray::fromBase64(
#include "opensans_regular.ttf.base64.hpp"
    );

    const auto opensans_semibold = QByteArray::fromBase64(
#include "opensans_semibold.ttf.base64.hpp"
    );

    const auto robotomono_regular = QByteArray::fromBase64(
#include "robotomono_regular.ttf.base64.hpp"
    );

    const auto materialicons_regular = QByteArray::fromBase64(
#include "materialicons_regular.ttf.base64.hpp"
    );
}
