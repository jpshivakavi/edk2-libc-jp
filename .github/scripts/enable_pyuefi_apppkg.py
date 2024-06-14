'''Script to enable the build of python UEFI interpreter 
   in AppPkg.dsc file
'''
import os


script_path = os.path.abspath(__file__)
script_dir = os.path.dirname(script_path)

# path to the AppPkg.dsc file
path_to_AppPkg_dsc = os.path.join(script_dir, '..', '..', 'edk2', 'AppPkg', 'AppPkg.dsc')
print('Path to AppPkg dsc file : ', path_to_AppPkg_dsc)

# Check if the file exists
if not os.path.isfile(path_to_AppPkg_dsc):
    print(f"The file {path_to_AppPkg_dsc} does not exist.")
else:
    # Read the content of the file
    with open(path_to_AppPkg_dsc, 'r') as file:
        lines = file.readlines()

    # Uncomment the line containing "Python368.inf"
    with open(path_to_AppPkg_dsc, 'w') as file:
        for line in lines:
            if 'Python368.inf' in line and line.strip().startswith('#'):
                # Uncomment the line
                file.write(line.lstrip('#'))
            else:
                file.write(line)

    print(f"The file {path_to_AppPkg_dsc} has been updated.")
