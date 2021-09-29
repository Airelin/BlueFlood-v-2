import os
import progressbar
import numpy as np

import matplotlib.pyplot as plt
from utility import slugify, cached, init_cache, load_env_config

METHOD_PREFIX = 'export_'

CONFIDENCE_FILL_COLOR = '0.8'
COLOR_MAP = 'tab10'


def load_plot_defaults():
    # Configure as needed
    plt.rc('lines', linewidth=2.0)
    plt.rc('legend', framealpha=1.0, fancybox=True)
    plt.rc('errorbar', capsize=3)
    plt.rc('pdf', fonttype=42)
    plt.rc('ps', fonttype=42)
    plt.rc('font', size=11)


def export_bar_example(config, export_dir):
    # we want to display two bar grahps
    # see export_sine_wave_example

    N=4 #Number of modes
    
    ############################ Old blueflood data ###########################################


    data_e = []
    with open('../Logs/Round-nr_Fixed/5309_round_fixed-new-blueflood-mode5/logs/join-hops.txt', encoding="iso-8859-14") as file_e:
        lines_e = file_e.readlines()
        str_num_e = []
        for line in lines_e:
            str_num_e.append(line)
        for num in str_num_e:
            data_e.append(float(num))
    
    data_f = []
    with open('../Logs/Round-nr_Fixed/5310_round_fixed-new-blueflood-mode6/logs/join-hops.txt', encoding="iso-8859-14") as file_f:
        lines_f = file_f.readlines()
        str_num_f = []
        for line in lines_f:
            str_num_f.append(line)
        for num in str_num_f:
            data_f.append(float(num))

    data_g = [] 
    with open('../Logs/Round-nr_Fixed/5307_round_fixed-new-blueflood-mode3/logs/join-hops.txt', encoding="iso-8859-14") as file_g:
        lines_g = file_g.readlines()
        str_num_g = []
        for line in lines_g:
            str_num_g.append(line)
        for num in str_num_g:
            data_g.append(float(num))

    data_h = []
    with open('../Logs/Round-nr_Fixed/5308_round_fixed-new-blueflood-mode4/logs/join-hops.txt', encoding="iso-8859-14") as file_h:
        lines_h = file_h.readlines()
        str_num_h = []
        for line in lines_h:
            str_num_h.append(line)
        for num in str_num_h:
            data_h.append(float(num))

    with open('../Logs/Round-nr_Fixed_run2/5365_round_fixed-new-blueflood-mode5/logs/join-hops.txt', encoding="iso-8859-14") as file_e1:
        lines_e1 = file_e1.readlines()
        str_num_e1 = []
        for line in lines_e1:
            str_num_e1.append(line)
        for num in str_num_e1:
            data_e.append(float(num))

    with open('../Logs/Round-nr_Fixed_run2/5366_round_fixed-new-blueflood-mode6/logs/join-hops.txt', encoding="iso-8859-14") as file_f1:
        lines_f1 = file_f1.readlines()
        str_num_f1 = []
        for line in lines_f1:
            str_num_f1.append(line)
        for num in str_num_f1:
            data_f.append(float(num))

    with open('../Logs/Round-nr_Fixed_run2/5363_round_fixed-new-blueflood-mode3/logs/join-hops.txt', encoding="iso-8859-14") as file_g1:
        lines_g1 = file_g1.readlines()
        str_num_g1 = []
        for line in lines_g1:
            str_num_g1.append(line)
        for num in str_num_g1:
            data_g.append(float(num))

    with open('../Logs/Round-nr_Fixed_run2/5364_round_fixed-new-blueflood-mode4/logs/join-hops.txt', encoding="iso-8859-14") as file_h1:
        lines_h1 = file_h1.readlines()
        str_num_h1 = []
        for line in lines_h1:
            str_num_h1.append(line)
        for num in str_num_h1:
            data_h.append(float(num))


    mean_e = np.mean(data_e)
    mean_f = np.mean(data_f)
    mean_g = np.mean(data_g)
    mean_h = np.mean(data_h)

    std_e = np.std(data_e)
    std_f = np.std(data_f)
    std_g = np.std(data_g)
    std_h = np.std(data_h)
    
    ############################ New blueflood data ###########################################
    

    ###################################### The plot ###################################################

    plt.clf()

    fig, ax = plt.subplots()

    ind = np.arange(N)  # the 4 modes
    width = 0.35 

    ax.yaxis.grid(True)
    plt.ylabel("Number of rounds")

    #rects1 = ax.bar(ind, [mean_a,mean_b,mean_c,mean_d], width, yerr=[std_a,std_b,std_c,std_d])
    rects2 = ax.bar(ind+width, [mean_e,mean_f,mean_g,mean_h], width, yerr=[std_e,std_f,std_g,std_h], color='#ff7f0e')


    ax.set_xticks(ind + width / 2)
    ax.set_xticklabels( ('125Kbps', '500Kbps', '1Mbps', '2Mbps') )
    plt.axis([None, None, 0, 150])

    # Adapt the figure size as needed
    fig.set_size_inches(5.0, 8.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210924","Join-hops","round","small", 5.0, 8.0)) + ".pdf", format="pdf")

    fig.set_size_inches(4.0, 4.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210924","join-hops","round","small", 4.0, 4.0)) + ".pdf", format="pdf")

    plt.close()


if __name__ == '__main__':

    config = load_env_config()

    load_plot_defaults()

    assert 'EXPORT_DIR' in config and config['EXPORT_DIR']

    if 'CACHE_DIR' in config and config['CACHE_DIR']:
        init_cache(config['CACHE_DIR'])

    steps = [
        #export_sine_wave_example,  # I put the example I am working on to the beginning
        export_bar_example,
        # export_example_3,  excluded for now
    ]

    for step in progressbar.progressbar(steps, redirect_stdout=True):
        name = step.__name__.removeprefix(METHOD_PREFIX)
        print("Handling {}".format(name))
        export_dir = os.path.join(config['EXPORT_DIR'], name) + '/'
        os.makedirs(export_dir, exist_ok=True)
        step(config, export_dir)
