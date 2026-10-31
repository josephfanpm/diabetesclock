#ifndef BGDISPLAYFACEWEATHER_H
#define BGDISPLAYFACEWEATHER_H

#include "BGDisplayFace.h"

// The configured city's three-letter code, a weather icon, and the temperature. No glucose.
class BGDisplayFaceWeather : public BGDisplayFace {
public:
    void showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld = false) const override;
    void showNoData() const override;
    RenderDecision getRenderDecision(const RenderContext& ctx) const override;

private:
    void showWeather() const;
    String getCityCode() const;
    mutable unsigned long shownUpdateCount = 0;
};

#endif  // BGDISPLAYFACEWEATHER_H
