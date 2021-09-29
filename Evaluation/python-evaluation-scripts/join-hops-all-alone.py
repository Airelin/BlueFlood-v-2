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


    data_a = []
    with open('../Logs/Default_20210923_run3/5369_default-old-blueflood-mode5/logs/join-hops.txt', encoding="iso-8859-14") as file_a:
        lines_a = file_a.readlines()
        str_num_a = []
        for line in lines_a:
            str_num_a.append(line)
        for num in str_num_a:
            data_a.append(float(num))
    
    data_b = []
    with open('../Logs/Default_20210923_run3/5370_default-old-blueflood-mode6/logs/join-hops.txt', encoding="iso-8859-14") as file_b:
        lines_b = file_b.readlines()
        str_num_b = []
        for line in lines_b:
            str_num_b.append(line)
        for num in str_num_b:
            data_b.append(float(num))

    data_c = [] 
    with open('../Logs/Default_20210923_run3/5367_default-old-blueflood-mode3/logs/join-hops.txt', encoding="iso-8859-14") as file_c:
        lines_c = file_c.readlines()
        str_num_c = []
        for line in lines_c:
            str_num_c.append(line)
        for num in str_num_c:
            data_c.append(float(num))

    data_d = []
    with open('../Logs/Default_20210923_run3/5368_default-old-blueflood-mode4/logs/join-hops.txt', encoding="iso-8859-14") as file_d:
        lines_d = file_d.readlines()
        str_num_d = []
        for line in lines_d:
            str_num_d.append(line)
        for num in str_num_d:
            data_d.append(float(num))

    with open('../Logs/Default_20210923_run2/5353_default-old-blueflood-mode5/logs/join-hops.txt', encoding="iso-8859-14") as file_a1:
        lines_a1 = file_a1.readlines()
        str_num_a1 = []
        for line in lines_a1:
            str_num_a1.append(line)
        for num in str_num_a1:
            data_a.append(float(num))
    
    with open('../Logs/Default_20210923_run2/5354_default-old-blueflood-mode6/logs/join-hops.txt', encoding="iso-8859-14") as file_b1:
        lines_b1 = file_b1.readlines()
        str_num_b1 = []
        for line in lines_b1:
            str_num_b1.append(line)
        for num in str_num_b1:
            data_b.append(float(num))

    with open('../Logs/Default_20210923_run2/5351_default-old-blueflood-mode3/logs/join-hops.txt', encoding="iso-8859-14") as file_c1:
        lines_c1 = file_c1.readlines()
        str_num_c1 = []
        for line in lines_c1:
            str_num_c1.append(line)
        for num in str_num_c1:
            data_c.append(float(num))

    with open('../Logs/Default_20210923_run2/5352_default-old-blueflood-mode4/logs/join-hops.txt', encoding="iso-8859-14") as file_d1:
        lines_d1 = file_d1.readlines()
        str_num_d1 = []
        for line in lines_d1:
            str_num_d1.append(line)
        for num in str_num_d1:
            data_d.append(float(num))

    with open('../Logs/Default_20210923/5313_default-old-blueflood-mode5/logs/join-hops.txt', encoding="iso-8859-14") as file_a2:
        lines_a2 = file_a2.readlines()
        str_num_a2 = []
        for line in lines_a2:
            str_num_a2.append(line)
        for num in str_num_a2:
            data_a.append(float(num))
    
    with open('../Logs/Default_20210923/5314_default-old-blueflood-mode6/logs/join-hops.txt', encoding="iso-8859-14") as file_b2:
        lines_b2 = file_b2.readlines()
        str_num_b2 = []
        for line in lines_b2:
            str_num_b2.append(line)
        for num in str_num_b2:
            data_b.append(float(num))

    with open('../Logs/Default_20210923/5311_default-old-blueflood-mode3/logs/join-hops.txt', encoding="iso-8859-14") as file_c2:
        lines_c2 = file_c2.readlines()
        str_num_c2 = []
        for line in lines_c2:
            str_num_c2.append(line)
        for num in str_num_c2:
            data_c.append(float(num))

    with open('../Logs/Default_20210923/5312_default-old-blueflood-mode4/logs/join-hops.txt', encoding="iso-8859-14") as file_d2:
        lines_d2 = file_d2.readlines()
        str_num_d2 = []
        for line in lines_d2:
            str_num_d2.append(line)
        for num in str_num_d2:
            data_d.append(float(num))
    
    mean_a = np.mean(data_a)
    mean_b = np.mean(data_b)
    mean_c = np.mean(data_c)
    mean_d = np.mean(data_d)

    std_a = np.std(data_a)
    std_b = np.std(data_b)
    std_c = np.std(data_c)
    std_d = np.std(data_d)
    

    ###################################### The plot ###################################################

    plt.clf()

    fig, ax = plt.subplots()

    ind = np.arange(N)  # the 4 modes
    width = 0.35 

    ax.yaxis.grid(True)
    plt.ylabel("Number of rounds")

    rects1 = ax.bar(ind, [mean_a,mean_b,mean_c,mean_d], width, yerr=[std_a,std_b,std_c,std_d])
    #rects2 = ax.bar(ind+width, [mean_e,mean_f,mean_g,mean_h], width, yerr=[std_e,std_f,std_g,std_h])


    ax.set_xticks(ind + width / 2)
    ax.set_xticklabels( ('125Kbps', '500Kbps', '1Mbps', '2Mbps') )
    plt.axis([None, None, 0, 1500])

    # Adapt the figure size as needed
    fig.set_size_inches(5.0, 8.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210924","Join-hops","default","Big", 5.0, 8.0)) + ".pdf", format="pdf")

    fig.set_size_inches(4.0, 4.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210924","join-hops","default","Big", 4.0, 4.0)) + ".pdf", format="pdf")

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
