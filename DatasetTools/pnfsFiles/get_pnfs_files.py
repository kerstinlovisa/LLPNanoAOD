import os

# Define the paths to the base directories
base_pnfs_path = '/pnfs/desy.de/cms/tier2'

redirector = 'root://dcache-cms-xrootd.desy.de'

dataset_dir = '/store/user/lrygaard/ttalps/TTToSemiLeptonic_TuneCP5_13TeV-powheg-pythia8/LLPnanoAODv1_LLPminiAOD'
subdirs = ['240725_165308']
output_file = 'TTToSemiLeptonic_LLPnanoAODv1_LLPminiAOD_2017.txt'

total_pnfs_files = []

for i in range(20):  # X = 0 to 20
    for subdir in subdirs:
        dir = f"{base_pnfs_path}{dataset_dir}/{subdir}/000{i}"
        print(f"Checking {dir}")
        if os.path.exists(dir):
            # list files in the directory
            files_dir = os.listdir(dir)
            for file in files_dir:
                full_path = f'{redirector}{base_pnfs_path}{dataset_dir}/{subdir}/000{i}/{file}'
                total_pnfs_files.append(full_path)

n_pnfs_files = len(total_pnfs_files)

with open(output_file, 'w') as f:
    for file_path in total_pnfs_files:
        f.write(f'{file_path}\n')

print(f"All {n_pnfs_files} pnfs files have been saved to {output_file}.")
