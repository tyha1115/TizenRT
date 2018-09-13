/******************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

/**
 * @ingroup MEDIA
 * @{
 */

/**
 * @file media/voice/SpeechDetectorInterface.h
 * @brief Media SpeechDetectorInterface APIs
 */

#ifndef __MEDIA_SPEECH_DETECTOR_INTERFACE_H
#define __MEDIA_SPEECH_DETECTOR_INTERFACE_H

#include <functional>
#include <memory>

namespace media {
namespace voice {

typedef std::function<void(void)> OnEndPointDetectedCallback;

class SpeechDetectorInterface
{
public:
	static SpeechDetectorInterface *instance();

public:
	virtual bool initKeywordDetect(uint32_t samprate, uint8_t channels) = 0;

	virtual bool initEndPointDetect(uint32_t samprate, uint8_t channels) = 0;

	virtual void deinitKeywordDetect() = 0;

	virtual void deinitEndPointDetect() = 0;

	virtual void setEndPointDetectedDelegate(
		OnEndPointDetectedCallback onEndPointDetected) = 0;

	virtual bool startKeywordDetect(uint32_t timeout) = 0;

	virtual bool processEPDFrame(short *sample, int numSample) = 0;
};

} // namespace voice
} // namespace media

#endif
