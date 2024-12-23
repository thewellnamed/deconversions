#!/bin/bash
curl -X POST -d "algo=wordnet-chains" -d "@$1" http://localhost:5000/api/
