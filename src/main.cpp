#include "main.hpp"

#include <string>


#include "GlobalNamespace/MainMenuViewController.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "UnityEngine/GameObject.hpp"

#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"

#include "extern/beatsaber-hook/shared/utils/typedefs.h"
#include "extern/beatsaber-hook/shared/utils/byref.hpp"

#include "extern/beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "extern/libil2cpp/il2cpp/libil2cpp/utils/StringUtils.h"


#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/DownloadHandler.hpp"
#include "UnityEngine/Networking/DownloadHandlerBuffer.hpp"
#include "UnityEngine/Networking/UploadHandler.hpp"
#include "UnityEngine/Networking/UploadHandlerRaw.hpp"

#include "System/Text/Encoding.hpp"


using namespace GlobalNamespace;

static ModInfo modInfo; // Stores the ID and version of our mod, and is sent to the modloader upon startup
static std::string selectedLevelId;
static std::string songHash;
static std::string selectedLevelName;
static std::string selectedLevelAuthor;
static std::string selectedLevelSongAuthor;

std::string URL;
unsigned long userId;


// Converts the int representing an IBeatmapDifficulty into a string
std::string difficultyToString(BeatmapDifficulty difficulty)  {
    switch(difficulty)  {
        case BeatmapDifficulty::Easy:
            return "Easy";
        case BeatmapDifficulty::Normal:
            return "Normal";
        case BeatmapDifficulty::Hard:
            return "Hard";
        case BeatmapDifficulty::Expert:
            return "Expert";
        case BeatmapDifficulty::ExpertPlus:
            return "Expert+";
    }
    return "Unknown";
}


void processResults(SinglePlayerLevelSelectionFlowCoordinator* self, LevelCompletionResults* levelCompletionResults, IDifficultyBeatmap* difficultyBeatmap, GameplayModifiers* gameplayModifiers, bool practice) {
    if (practice || levelCompletionResults->levelEndAction.value != 0 || levelCompletionResults->levelEndStateType.value != 1){
        getLogger().info("Level not cleared.");
        return;
    }
    
    std::string diff = difficultyToString(difficultyBeatmap->get_difficulty());
    
    //PlayerSpecificSettings
    getLogger().info("Completed Song: %s, %s, %d, %d, %s, %d", 
        selectedLevelId.c_str(),
        selectedLevelName.c_str(),
        difficultyBeatmap->get_difficultyRank(), //1
        difficultyBeatmap->get_difficulty().value, //0 ?
        diff.c_str(),
        levelCompletionResults->rawScore); //score

    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    doc.SetObject();
    doc.AddMember("userId", userId, alloc);
    doc.AddMember("levelId", selectedLevelId, alloc);
    doc.AddMember("songName", selectedLevelName, alloc);
    doc.AddMember("levelAuthor", selectedLevelAuthor, alloc);
    doc.AddMember("songAuthor", selectedLevelSongAuthor, alloc);
    doc.AddMember("difficulty", diff, alloc);
    doc.AddMember("difficultyRank", difficultyBeatmap->get_difficultyRank(), alloc);
    doc.AddMember("difficultyRaw", difficultyBeatmap->get_difficulty().value, alloc);
    doc.AddMember("score", levelCompletionResults->rawScore, alloc);
    doc.AddMember("modifiedScore", levelCompletionResults->modifiedScore, alloc);
    // TODO modifiers
    doc.AddMember("fullCombo", levelCompletionResults->fullCombo, alloc);
    doc.AddMember("leftSaberDistance", levelCompletionResults->leftSaberMovementDistance, alloc);
    doc.AddMember("leftHandDistance", levelCompletionResults->leftHandMovementDistance, alloc);
    doc.AddMember("rightSaberDistance", levelCompletionResults->rightSaberMovementDistance, alloc);
    doc.AddMember("rightHandDistance", levelCompletionResults->rightHandMovementDistance, alloc);
    doc.AddMember("goodCuts", levelCompletionResults->goodCutsCount, alloc);
    doc.AddMember("badCuts", levelCompletionResults->badCutsCount, alloc);
    doc.AddMember("missed", levelCompletionResults->missedCount, alloc);
    doc.AddMember("notGood", levelCompletionResults->notGoodCount, alloc);
    doc.AddMember("ok", levelCompletionResults->okCount, alloc);
    doc.AddMember("averageCutScore", levelCompletionResults->averageCutScore, alloc);
    doc.AddMember("maxCutScore", levelCompletionResults->maxCutScore, alloc);
    doc.AddMember("averageCutDistanceRawScore", levelCompletionResults->averageCutDistanceRawScore, alloc);
    doc.AddMember("maxCombo", levelCompletionResults->maxCombo, alloc);
    doc.AddMember("minDirDeviation", levelCompletionResults->minDirDeviation, alloc);
    doc.AddMember("maxDirDeviation", levelCompletionResults->maxDirDeviation, alloc);
    doc.AddMember("averageDirDeviation", levelCompletionResults->averageDirDeviation, alloc);
    doc.AddMember("minTimeDeviation", levelCompletionResults->minTimeDeviation, alloc);
    doc.AddMember("maxTimeDeviation", levelCompletionResults->maxTimeDeviation, alloc);
    doc.AddMember("averageTimeDeviation", levelCompletionResults->averageTimeDeviation, alloc);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);    
    doc.Accept(writer);
    std::string body = buffer.GetString();
    getLogger().info("Score Submit Body: %s", body.c_str()); 
    
    auto webRequest2 = UnityEngine::Networking::UnityWebRequest::New_ctor(il2cpp_utils::newcsstr(URL), il2cpp_utils::newcsstr("POST"), 
            UnityEngine::Networking::DownloadHandlerBuffer::New_ctor(), 
            UnityEngine::Networking::UploadHandlerRaw::New_ctor(System::Text::Encoding::get_ASCII()->GetBytes(il2cpp_utils::newcsstr(body))));

    webRequest2->SetRequestHeader(il2cpp_utils::newcsstr("Content-Type"), il2cpp_utils::newcsstr("application/json"));
    webRequest2->SendWebRequest();
}


MAKE_HOOK_MATCH(ProcessResultsSolo, &SoloFreePlayFlowCoordinator::ProcessLevelCompletionResultsAfterLevelDidFinish, void, SoloFreePlayFlowCoordinator* self, LevelCompletionResults* levelCompletionResults, IDifficultyBeatmap* difficultyBeatmap, GameplayModifiers* gameplayModifiers, bool practice) {
    ProcessResultsSolo(self, levelCompletionResults, difficultyBeatmap, gameplayModifiers, practice);
    processResults(self, levelCompletionResults, difficultyBeatmap, gameplayModifiers, practice);
}


MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);
    IPreviewBeatmapLevel* level = reinterpret_cast<IPreviewBeatmapLevel*>(self->level);
    if(!level) {
        return;
    }

    // Check if the level is an instance of BeatmapLevelSO
    selectedLevelId = to_utf8(csstrtostr(level->get_levelID()));
    songHash = selectedLevelId.rfind("custom_level_", 0) == 0 ? selectedLevelId.substr(13) : "NO HASH";
    selectedLevelName = to_utf8(csstrtostr(level->get_songName())).c_str();
    selectedLevelAuthor = to_utf8(csstrtostr(level->get_levelAuthorName())).c_str();
    selectedLevelSongAuthor = to_utf8(csstrtostr(level->get_songAuthorName())).c_str();
    getLogger().info("Song Info: %s, %s, %s, %s, %s", selectedLevelId.c_str(), songHash.c_str(), selectedLevelName.c_str(), selectedLevelAuthor.c_str(), selectedLevelSongAuthor.c_str());
}

// Loads the config from disk using our modInfo, then returns it for use
Configuration& getConfig() {
    static Configuration config(modInfo);
    return config;
}

// Returns a logger, useful for printing debug messages
Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

void saveDefaultConfig() {
    getLogger().info("Creating config file . . .");
    ConfigDocument& config = getConfig().config;
    auto& alloc = config.GetAllocator();
    // If the config has already been created, don't overwrite it
   
    if(config.HasMember("submitData")) {
        getLogger().info("Config file already exists");
        return;
    }
    config.RemoveAllMembers();
    config.SetObject();
    // Create the sections of the config file for each type of presence
    rapidjson::Value submitData(rapidjson::kObjectType);
    submitData.AddMember("userId", 12345, alloc);
    submitData.AddMember("url",  "http://192.168.86.207:8081/score", alloc);
    config.AddMember("submitData", submitData, alloc);

    getConfig().Write();
    getLogger().info("Config file created");
}

// Called at the early stages of game loading
extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
	
    getConfig().Load(); // Load the config file
    saveDefaultConfig();
    
    getLogger().info("Completed setup!");
}

// Called later on in the game loading - a good time to install function hooks
extern "C" void load() {
    il2cpp_functions::Init();    
    
    // MUST BE IN UNIX_STYLE LINE ENDINGS!
    ConfigDocument& config = getConfig().config;
    URL = config["submitData"]["url"].GetString();
    userId = config["submitData"]["userId"].GetUint();
    getLogger().info("Discord Config: %s %ld", URL.c_str(), userId);
    
    getLogger().info("Installing hooks...");
    // Install our hooks 
    INSTALL_HOOK(getLogger(), ProcessResultsSolo);
    INSTALL_HOOK(getLogger(), StandardLevelDetailView_RefreshContent);
    getLogger().info("Installed all hooks!");
}