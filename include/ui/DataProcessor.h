#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include <vector>
#include <map>
#include "core/SharedTypes.h"
#include "ui/PlotDataTypes.h"
#include "ui/PlotManager.h" // For ColorPair access or we can move ColorPair
#include "core/Config.h"

class DataProcessor {
public:
    static DashboardData process(
        const std::vector<SL_Pet> &pets, 
        const PetDataMap &allPetData, 
        const DateRangeInfo &range,
        const std::vector<ColorPair>& colors // Pass colors explicitly
    );

    static DashboardData processEnvData(
        const std::vector<env_data>& envData,
        const DateRangeInfo& range,
        const std::vector<ColorPair>& colors
    );
};

#endif // DATA_PROCESSOR_H
