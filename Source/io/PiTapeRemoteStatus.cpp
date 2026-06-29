#include "PiTapeRemoteStatus.h"

namespace cassette
{

namespace
{

PiTapeSideQueueState sideFromVar(const juce::var& v)
{
    PiTapeSideQueueState s;
    if (auto* o = v.getDynamicObject())
    {
        s.ready = static_cast<bool>(o->getProperty("ready"));
        s.played = static_cast<bool>(o->getProperty("played"));
        s.side = o->getProperty("side").toString();
        s.filename = o->getProperty("filename").toString();
        s.durationSec = static_cast<double>(o->getProperty("duration_sec"));
    }
    return s;
}

} // namespace

PiTapeRemoteStatus PiTapeRemoteStatus::fromJson(const juce::String& jsonText)
{
    PiTapeRemoteStatus status;
    const auto parsed = juce::JSON::parse(jsonText);
    if (parsed.isVoid())
    {
        status.error = "Invalid JSON";
        return status;
    }

    status.online = true;
    if (auto* root = parsed.getDynamicObject())
    {
        status.queueJobId = root->getProperty("queue_job_id").toString();
        status.projectName = root->getProperty("project_name").toString();

        if (auto* playback = root->getProperty("playback").getDynamicObject())
        {
            status.playback.state = playback->getProperty("state").toString();
            status.playback.side = playback->getProperty("side").toString();
            status.playback.positionSec = static_cast<double>(playback->getProperty("position_sec"));
            status.playback.durationSec = static_cast<double>(playback->getProperty("duration_sec"));
        }

        if (auto* queue = root->getProperty("queue").getDynamicObject())
        {
            status.sideA = sideFromVar(queue->getProperty("side_a"));
            status.sideB = sideFromVar(queue->getProperty("side_b"));
        }
    }

    return status;
}

} // namespace cassette
