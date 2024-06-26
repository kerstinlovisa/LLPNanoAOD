#!/bin/bash

export X509_USER_PROXY=`pwd`/voms_proxy

export VO_CMS_SW_DIR=/cvmfs/cms.cern.ch
source $VO_CMS_SW_DIR/cmsset_default.sh
export SCRAM_ARCH=slc7_amd64_gcc700
export CMSSW_GIT_REFERENCE=/cvmfs/cms.cern.ch/cmssw.git

cd $CMSSW_BASE
eval `scramv1 runtime -sh`

job_number=$1
echo "Executing job number $job_number"

home_path="<home_path>"
export HOME=$home_path

dataset=<dataset>

inputFiles_path="${CMSSW_BASE}/src/<input_files_path>"
all_files=()
while IFS= read -r filename; do
  all_files+=("$filename")
done < "$inputFiles_path"

output="<output_path>"
outputPath="${output}_part-${job_number}.root"

LLPminiAOD="<LLPminiAOD_path>"
LLPminiAOD_path="${LLPminiAOD}_part-${job_number}.root"

nEvents=<n_events>
nFiles=<n_files>
runOnData=<is_data>
runOnDAS=<run_on_das>

includeDSAMuon=<include_DSAMuon>
includeBS=<include_BS>
includeGenPart=<include_GenPart>
includeDGLMuon=<include_DGLMuon>
includeRefittedTracks=<include_refittedTracks>

saveLLPminiAOD=<save_LLPminiAOD>

total_files=${#all_files[@]}

for ((i=0; i<$nFiles; i++)); do
  file_number=$((job_number*nFiles+i))
  # Check that the file number doesn't exceed total number of files
    if [ $file_number -le $total_files ]; then 

      filename=${all_files[file_number]}
      if [ $i -eq 0 ]; then
        if [ -z "$filename" ]; then
          echo "Error: filename from dataset $dataset could not be found - exiting."
          exit 1
        fi
      fi
      if [[ $runOnDAS == "True" ]]; then
        # input_path="root://xrootd-cms.infn.it/$filename"
        input_path="root://cms-xrd-global.cern.ch/$filename"
      else
        input_path="file:$dataset/$filename"
      fi
      filename_list+="$input_path,"
    fi
done
# Remove the trailing comma
filename_list=${filename_list%,}

cd <work_dir>

cmsRun $CMSSW_BASE/src/LLPNanoAOD/LLPnanoAOD/test/LLPminiAOD_cfg.py "inputFiles=$filename_list" "outputFile=$LLPminiAOD_path" "nEvents=$nEvents" "runOnData=$runOnData"

cmsRun $CMSSW_BASE/src/LLPNanoAOD/LLPnanoAOD/test/LLPnanoAOD_cfg.py "inputFiles=file:$LLPminiAOD_path" "outputFile=$outputPath" "nEvents=$nEvents" "runOnData=$runOnData" "includeDSAMuon=$includeDSAMuon" "includeBS=$includeBS" "includeGenPart=$includeGenPart" "includeDGLMuon=$includeDGLMuon" "includeRefittedTracks=$includeRefittedTracks" 

echo "LLPNanoAOD file saved in: $outputPath"
if [[ $saveLLPminiAOD == "False" ]]; then
  echo rm $LLPminiAOD_path
  rm $LLPminiAOD_path
fi
echo "Done"