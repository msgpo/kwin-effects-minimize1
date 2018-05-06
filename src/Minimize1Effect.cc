/*
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Minimize1Effect.h"

// KConfigSkeleton
#include "minimize1config.h"

Minimize1Effect::Minimize1Effect()
{
    initConfig<Minimize1Config>();
    reconfigure(ReconfigureAll);

    connect(KWin::effects, &KWin::EffectsHandler::windowMinimized,
        this, &Minimize1Effect::start);
    connect(KWin::effects, &KWin::EffectsHandler::windowUnminimized,
        this, &Minimize1Effect::stop);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted,
        this, &Minimize1Effect::stop);
}

Minimize1Effect::~Minimize1Effect()
{
}

void Minimize1Effect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags);

    Minimize1Config::self()->read();
    m_duration = animationTime(Minimize1Config::duration() > 0
            ? Minimize1Config::duration()
            : 160);
    m_opacity = Minimize1Config::opacity();
    m_scale = Minimize1Config::scale();
}

void Minimize1Effect::prePaintScreen(KWin::ScreenPrePaintData& data, int time)
{
    auto it = m_animations.begin();
    while (it != m_animations.end()) {
        Timeline& t = *it;
        t.update(time);
        if (t.done()) {
            it = m_animations.erase(it);
        } else {
            ++it;
        }
    }

    if (!m_animations.isEmpty()) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }

    KWin::effects->prePaintScreen(data, time);
}

void Minimize1Effect::prePaintWindow(KWin::EffectWindow* w, KWin::WindowPrePaintData& data, int time)
{
    if (m_animations.contains(w)) {
        w->enablePainting(KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        data.setTransformed();
    }

    KWin::effects->prePaintWindow(w, data, time);
}

void Minimize1Effect::paintWindow(KWin::EffectWindow* w, int mask, QRegion region, KWin::WindowPaintData& data)
{
    const auto it = m_animations.constFind(w);
    if (it == m_animations.cend()) {
        KWin::effects->paintWindow(w, mask, region, data);
        return;
    }

    const QRect geometry(w->geometry());
    const QRect icon(w->iconGeometry());

    if (icon.isValid() && geometry.isValid()) {
        const qreal t = (*it).value();
        const qreal opacityProgress = t;
        const qreal scaleProgress = interpolate(0, m_scale, t);

        const qreal targetScale = icon.width() > icon.height()
            ? static_cast<qreal>(icon.width()) / geometry.width()
            : static_cast<qreal>(icon.height()) / geometry.height();

        data.setXScale(interpolate(1, targetScale, scaleProgress));
        data.setYScale(interpolate(1, targetScale, scaleProgress));
        data.setXTranslation(interpolate(0, icon.x() - geometry.x(), scaleProgress));
        data.setYTranslation(interpolate(0, icon.y() - geometry.y(), scaleProgress));

        data.multiplyOpacity(interpolate(1, m_opacity, opacityProgress));
    }

    KWin::effects->paintWindow(w, mask, region, data);
}

void Minimize1Effect::postPaintScreen()
{
    if (!m_animations.isEmpty()) {
        KWin::effects->addRepaintFull();
    }

    KWin::effects->postPaintScreen();
}

bool Minimize1Effect::isActive() const
{
    return !m_animations.isEmpty();
}

bool Minimize1Effect::supported()
{
    return KWin::effects->animationsSupported();
}

void Minimize1Effect::start(KWin::EffectWindow* w)
{
    if (KWin::effects->activeFullScreenEffect() != nullptr) {
        return;
    }

    Timeline& t = m_animations[w];
    t.setDuration(m_duration);
    t.setEasingCurve(QEasingCurve::OutCurve);
}

void Minimize1Effect::stop(KWin::EffectWindow* w)
{
    m_animations.remove(w);
}
