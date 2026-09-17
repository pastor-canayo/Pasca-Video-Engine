// PascaEditor.cpp : Defines the entry point for the application.
//

#include "PascaEditor.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

// 1. THE MEDIA ASSET COMPONENT: Tracks physical video properties
struct MediaAsset {
    string id;
    string name;
    string filePath;
    double totalDurationSeconds;
};

// 2. THE TIMELINE CLIP COMPONENT: Tracks sliced pieces, positions, and crop trims
struct VideoClip {
    string id;
    string mediaAssetId;
    double timelineStartSeconds; // Where it sits on the track row
    double clipDurationSeconds;  // How long the visual box is
    double sourceTrimInSeconds;  // The starting cut point inside the raw file

    bool isPlayheadIntersecting(double playheadTime) const {
        return playheadTime >= timelineStartSeconds && playheadTime < (timelineStartSeconds + clipDurationSeconds);
    }
};

// 3. THE MASTER TRACK LAYER LAYER: Manages sequences with zero overhead
class VideoTrackLane {
public:
    string trackId;
    string trackName; // e.g., "V1"
    vector<VideoClip> clips;

    void deleteClip(const string& clipId) {
        clips.erase(remove_if(clips.begin(), clips.end(),
            [&](const VideoClip& c) { return c.id == clipId; }), clips.end());
        closeTimelineGaps();
    }

    void closeTimelineGaps() {
        double currentX = 0.0;
        for (auto& clip : clips) {
            clip.timelineStartSeconds = currentX;
            currentX += clip.clipDurationSeconds;
        }
    }
};

int main()
{
    cout << "[Engine Initialized] PascaEditor Heavyweight Native Core Active." << endl;
    return 0;
}
