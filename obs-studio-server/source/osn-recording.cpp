/******************************************************************************
    Copyright (C) 2016-2022 by Streamlabs (General Workings Inc)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include "osn-recording.hpp"
#include "osn-video-encoder.hpp"
#include "osn-error.hpp"
#include "shared.hpp"
#include "util/platform.h"
#include <osn-video.hpp>

extern char* osn_generate_formatted_filename(const char* extension, bool space, const char* format, int width, int height);

osn::Recording::~Recording()
{
    deleteOutput();
}

void osn::IRecording::GetVideoEncoder(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
    Recording* recording =
        static_cast<Recording*>(
            osn::IFileOutput::Manager::GetInstance().find(args[0].value_union.ui64));
    if (!recording) {
        PRETTY_ERROR_RETURN(ErrorCode::InvalidReference, "Simple recording reference is not valid.");
    }

    uint64_t uid =
        osn::VideoEncoder::Manager::GetInstance().find(recording->videoEncoder);

    rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
    rval.push_back(ipc::value(uid));
    AUTO_DEBUG;
}

void osn::IRecording::SetVideoEncoder(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
    Recording* recording =
        static_cast<Recording*>(
            osn::IFileOutput::Manager::GetInstance().find(args[0].value_union.ui64));
    if (!recording) {
        PRETTY_ERROR_RETURN(ErrorCode::InvalidReference, "Simple recording reference is not valid.");
    }

    obs_encoder_t* encoder =
        osn::VideoEncoder::Manager::GetInstance().find(args[1].value_union.ui64);
    if (!encoder) {
        PRETTY_ERROR_RETURN(ErrorCode::InvalidReference, "Encoder reference is not valid.");
    }

    recording->videoEncoder = encoder;

    rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
    AUTO_DEBUG;
}

void osn::IRecording::GetVideoCanvas(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
    Recording* recording =
        static_cast<Recording*>(
            osn::IFileOutput::Manager::GetInstance().find(args[0].value_union.ui64));
    if (!recording) {
        PRETTY_ERROR_RETURN(ErrorCode::InvalidReference, "Simple recording reference is not valid.");
    }

    uint64_t uid =
        osn::Video::Manager::GetInstance().find(recording->canvas);

    rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
    rval.push_back(ipc::value(uid));
    AUTO_DEBUG;
}

void osn::IRecording::SetVideoCanvas(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
    Recording* recording =
        static_cast<Recording*>(
            osn::IFileOutput::Manager::GetInstance().find(args[0].value_union.ui64));
    if (!recording) {
        PRETTY_ERROR_RETURN(ErrorCode::InvalidReference, "Simple recording reference is not valid.");
    }

    obs_video_info* canvas =
        osn::Video::Manager::GetInstance().find(args[1].value_union.ui64);
    if (!canvas) {
        PRETTY_ERROR_RETURN(ErrorCode::InvalidReference, "Canvas reference is not valid.");
    }

    recording->canvas = canvas;

    rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
    AUTO_DEBUG;
}

void osn::IRecording::Query(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
    Recording* recording =
        static_cast<Recording*>(
            osn::IFileOutput::Manager::GetInstance().find(args[0].value_union.ui64));
    if (!recording) {
        PRETTY_ERROR_RETURN(
            ErrorCode::InvalidReference, "Recording reference is not valid.");
    }

    std::unique_lock<std::mutex> ulock(recording->signalsMtx);
    if (recording->signalsReceived.empty()) {
        rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
        AUTO_DEBUG;
        return;
    }

    rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));

    auto signal = recording->signalsReceived.front();
    rval.push_back(ipc::value("recording"));
    rval.push_back(ipc::value(signal.signal));
    rval.push_back(ipc::value(signal.code));
    rval.push_back(ipc::value(signal.errorMessage));

    recording->signalsReceived.pop();

    AUTO_DEBUG;
}

std::string osn::IRecording::GenerateSpecifiedFilename(
    const std::string& extension, bool noSpace, const std::string& format, int width, int height)
{
    char* filename = osn_generate_formatted_filename(extension.c_str(), !noSpace, format.c_str(), width, height);
    if (filename == nullptr) {
        throw "Invalid filename";
    }
    
    std::string result(filename);

    bfree(filename);

    return result;
}

void osn::IRecording::FindBestFilename(std::string& strPath, bool noSpace)
{
    int num = 2;

    if (!os_file_exists(strPath.c_str()))
        return;

    const char* ext = strrchr(strPath.c_str(), '.');
    if (!ext)
        return;

    int extStart = int(ext - strPath.c_str());
    for (;;) {
        std::string testPath = strPath;
        std::string numStr;

        numStr = noSpace ? "_" : " (";
        numStr += std::to_string(num++);
        if (!noSpace)
            numStr += ")";

        testPath.insert(extStart, numStr);

        if (!os_file_exists(testPath.c_str())) {
            strPath = testPath;
            break;
        }
    }
}

obs_encoder_t* osn::IRecording::duplicate_encoder(obs_encoder_t* src, uint64_t trackIndex)
{
    if (!src)
        return nullptr;

    obs_encoder_t* dst = nullptr;
    std::string name = obs_encoder_get_name(src);
    name += "-duplicate";

    if (obs_encoder_get_type(src) == OBS_ENCODER_AUDIO) {
        dst = obs_audio_encoder_create(
            obs_encoder_get_id(src),
            name.c_str(),
            obs_encoder_get_settings(src), trackIndex, nullptr);
    } else if (obs_encoder_get_type(src) == OBS_ENCODER_VIDEO) {
        dst = obs_video_encoder_create(
            obs_encoder_get_id(src),
            name.c_str(),
            obs_encoder_get_settings(src), nullptr);
    }

    return dst;
}