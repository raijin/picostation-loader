#!/bin/bash
# Script equivalent to makeimage.bat

# Run the MKPSxiso with the configuration file
mkpsxiso -y ../isoconfig.xml

# Successful message
echo "ISO image generated successfully."
