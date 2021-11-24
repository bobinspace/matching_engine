#!/bin/bash
docker build -f Dockerfile -t gemini_interview . && docker run -i --rm gemini_interview /matching_engine/build/test/me_test --durations yes

