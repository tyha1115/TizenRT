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
#ifndef __MEDIA_SPEECHDETECTOR_H
#define __MEDIA_SPEECHDETECTOR_H

#include <functional>
#include <memory>

#include <media/voice/SpeechDetectorInterface.h>

#include "KeywordDetector.h"
#include "EndPointDetector.h"

namespace media {
namespace voice {

class SpeechDetector : public SpeechDetectorInterface
{
public:
	SpeechDetector();

public:
	bool initKeywordDetect(uint32_t samprate, uint8_t channels) override;

	bool initEndPointDetect(uint32_t samprate, uint8_t channels) override;

	void deinitKeywordDetect() override;

	void deinitEndPointDetect() override;

	void setEndPointDetectedDelegate(
		OnEndPointDetectedCallback onEndPointDetected) override;

	bool startKeywordDetect(uint32_t timeout) override;

	bool processEPDFrame(short *sample, int numSample) override;

private:
	std::shared_ptr<KeywordDetector> mKeywordDetector;
	std::shared_ptr<EndPointDetector> mEndPointDetector;

	int mKeywordDetectorCard;
	int mKeywordDetectorDevice;

	int mEndPointDetectorCard;
	int mEndPointDetectorDevice;
};

} // namespace voice
} // namespace media

#endif
