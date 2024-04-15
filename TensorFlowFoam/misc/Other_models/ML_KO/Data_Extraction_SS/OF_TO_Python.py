import numpy as np
import matplotlib.pyplot as plt
import string

# Reproducibility
np.random.seed(10)

# Paths
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
PARENT = os.path.dirname(os.getcwd())
BASE_DIR = os.path.dirname(PARENT)
DATA_DIR = PARENT+r'/KO_Run/'
sys.path.insert(0,BASE_DIR)

# Import functions for reading OF output
from OF_Readers.read_strain_rotation import read_rot_file
from OF_Readers.read_nut import read_nut_grad_magnitude, read_nut_file, read_nut_grad
from OF_Readers.read_p import read_p_gradient, read_p_file
from OF_Readers.read_velocity import read_velocity_magnitude, read_velocity_grad_magnitude, read_velocity
from OF_Readers.read_y import read_y_file
from OF_Readers.prune_data import prune_data
from OF_Readers.read_vorticity import read_vorticity_z

def concat_sampling(NOW_DIR,NEXT_DIR,uref):
    u = read_velocity(NOW_DIR+'U')#/uref
    # p = read_p_file(NOW_DIR+'p')
    ycoods = read_y_file(NOW_DIR+'Cy')
    xcoods = read_y_file(NEXT_DIR+'Cx')
    nut_next = read_nut_file(NEXT_DIR+'nut')

    dataset = np.concatenate((u,ycoods,xcoods,nut_next),axis=1)

    return dataset       


if __name__ == '__main__':

    mode = 'generate'
    prune = 'no'
    prune_val = 4000

    if mode == 'generate':
        print('Generating new data:')
        
        # Reading TG_1 - 44.2
        input_dir = DATA_DIR+r'/0/'
        output_dir = DATA_DIR+r'/871/'
        dataset = concat_sampling(input_dir,output_dir,44.2)

        print('Final dataset shape',np.shape(dataset))
        np.save('Total_Data_SS.npy',dataset)
    else:
        dataset = np.load('Total_Data_SS.npy')
        print('Loading pre-generated data')
        print('Final dataset shape',np.shape(dataset))

    means = np.mean(dataset,axis=0).reshape(1,np.shape(dataset)[1])
    stds = np.std(dataset,axis=0).reshape(1,np.shape(dataset)[1])

    np.savetxt('Means.txt',means,delimiter=',')
    np.savetxt('Stds.txt',stds,delimiter=',')
    
    print('Means:')
    print(means)
    print('Stds:')
    print(stds)

    diff_nut_all = dataset[:,-1].flatten()
    plt.figure()
    plt.hist(diff_nut_all,bins=100,label='nut')
    plt.show()


