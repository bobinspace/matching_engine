#!/bin/bash
docker build -f Dockerfile -t gemini_interview .
docker run -i --rm gemini_interview /matching_engine/build/src/me_app
