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


def export_sine_wave_example(config, export_dir):
    # We got multiple experiment runs with individual measurements
    num_runs = 10

    # Create our measurement steps
    xs = np.linspace(0, 2 * np.pi, 100, endpoint=True)

    # We also collect overall data for mean and confidence interval
    overall_data = []

    for r in range(0, num_runs):
        name = "Sine Wave Run {}".format(r)

        def proc():
            # you can load your data from a database or CSV file here
            # we will randomly generate data
            ys = np.sin(np.array(xs))
            # we add some uniform errors
            ys += np.random.uniform(-0.1, 0.1, len(xs))
            return ys

        # If caching is enabled, this line checks for available cache data
        # If no data was found, the proc callback is executed and the result cached
        # Use ys = proc() if caching not yet wanted
        ys = cached(('sine_wave', r), proc)

        # We also add the data to overall_data
        overall_data.append(ys)

        plt.clf()

        # Plot the main data
        plt.plot(xs, ys, linestyle='-', label="Sin Run {}".format(r), color='C' + str(r + 1))

        plt.legend()
        plt.xlabel("x")
        plt.ylabel("sin(x)")
        plt.axis([None, None, None, None])
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(export_dir + slugify(name) + ".pdf", format="pdf")
        plt.close()

    overall_data = np.array(overall_data)

    # We swap the axes to get all values at the first position together
    overall_data = np.swapaxes(overall_data, 0, 1)

    # We can then merge each step to get the mean
    mean = np.mean(overall_data, axis=1)

    # Calculate the lower and upper bounds of the confidence interval
    # This describes that 95% of the measurements (for each timestep) are within that range
    # Use standard error to determine the "quality" of your calculated mean
    (lq, uq) = np.percentile(overall_data, [2.5, 97.5], axis=1)

    plt.clf()
    plt.plot(xs, mean, linestyle='-', label="Mean", color='C1')
    plt.fill_between(xs, lq, uq, color=CONFIDENCE_FILL_COLOR, label='95% Confidence Interval')
    plt.legend()
    plt.xlabel("x")
    plt.ylabel("sin(x)")
    plt.axis([None, None, None, None])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(export_dir + slugify("Sine Wave Mean") + ".pdf", format="pdf")
    plt.close()


def export_bar_example(config, export_dir):
    # we want to display two bar grahps
    # see export_sine_wave_example

    N=4 #Number of modes

    data_a = []
    with open('../Logs/Default_20210923_run3/5369_default-old-blueflood-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_a:
        lines_a = file_a.readlines()
        str_num_a = []
        for line in lines_a:
            str_num_a.append(line)
        for num in str_num_a:
            data_a.append(float(num))
    
    data_b = []
    with open('../Logs/Default_20210923_run3/5370_default-old-blueflood-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_b:
        lines_b = file_b.readlines()
        str_num_b = []
        for line in lines_b:
            str_num_b.append(line)
        for num in str_num_b:
            data_b.append(float(num))

    data_c = [] 
    with open('../Logs/Default_20210923_run3/5367_default-old-blueflood-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_c:
        lines_c = file_c.readlines()
        str_num_c = []
        for line in lines_c:
            str_num_c.append(line)
        for num in str_num_c:
            data_c.append(float(num))

    data_d = []
    with open('../Logs/Default_20210923_run3/5368_default-old-blueflood-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_d:
        lines_d = file_d.readlines()
        str_num_d = []
        for line in lines_d:
            str_num_d.append(line)
        for num in str_num_d:
            data_d.append(float(num))

    with open('../Logs/Default_20210923_run2/5353_default-old-blueflood-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_a1:
        lines_a1 = file_a1.readlines()
        str_num_a1 = []
        for line in lines_a1:
            str_num_a1.append(line)
        for num in str_num_a1:
            data_a.append(float(num))
    
    with open('../Logs/Default_20210923_run2/5354_default-old-blueflood-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_b1:
        lines_b1 = file_b1.readlines()
        str_num_b1 = []
        for line in lines_b1:
            str_num_b1.append(line)
        for num in str_num_b1:
            data_b.append(float(num))

    with open('../Logs/Default_20210923_run2/5351_default-old-blueflood-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_c1:
        lines_c1 = file_c1.readlines()
        str_num_c1 = []
        for line in lines_c1:
            str_num_c1.append(line)
        for num in str_num_c1:
            data_c.append(float(num))

    with open('../Logs/Default_20210923_run2/5352_default-old-blueflood-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_d1:
        lines_d1 = file_d1.readlines()
        str_num_d1 = []
        for line in lines_d1:
            str_num_d1.append(line)
        for num in str_num_d1:
            data_d.append(float(num))

    with open('../Logs/Default_20210923/5313_default-old-blueflood-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_a2:
        lines_a2 = file_a2.readlines()
        str_num_a2 = []
        for line in lines_a2:
            str_num_a2.append(line)
        for num in str_num_a2:
            data_a.append(float(num))
    
    with open('../Logs/Default_20210923/5314_default-old-blueflood-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_b2:
        lines_b2 = file_b2.readlines()
        str_num_b2 = []
        for line in lines_b2:
            str_num_b2.append(line)
        for num in str_num_b2:
            data_b.append(float(num))

    with open('../Logs/Default_20210923/5311_default-old-blueflood-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_c2:
        lines_c2 = file_c2.readlines()
        str_num_c2 = []
        for line in lines_c2:
            str_num_c2.append(line)
        for num in str_num_c2:
            data_c.append(float(num))

    with open('../Logs/Default_20210923/5312_default-old-blueflood-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_d2:
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
    
    ############################ New blueflood data ###########################################
    data_e = []
    with open('../Logs/Default_20210923_run3/5373_default-new-blueflood-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_e:
        lines_e = file_e.readlines()
        str_num_e = []
        for line in lines_e:
            str_num_e.append(line)
        for num in str_num_e:
            data_e.append(float(num))
    
    data_f = []
    with open('../Logs/Default_20210923_run3/5374_default-new-blueflood-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_f:
        lines_f = file_f.readlines()
        str_num_f = []
        for line in lines_f:
            str_num_f.append(line)
        for num in str_num_f:
            data_f.append(float(num))

    data_g = [] 
    with open('../Logs/Default_20210923_run3/5371_default-new-blueflood-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_g:
        lines_g = file_g.readlines()
        str_num_g = []
        for line in lines_g:
            str_num_g.append(line)
        for num in str_num_g:
            data_g.append(float(num))

    data_h = []
    with open('../Logs/Default_20210923_run3/5372_default-new-blueflood-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_h:
        lines_h = file_h.readlines()
        str_num_h = []
        for line in lines_h:
            str_num_h.append(line)
        for num in str_num_h:
            data_h.append(float(num))

    with open('../Logs/Default_20210923_run2/5357_default-new-blueflood-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_e1:
        lines_e1 = file_e1.readlines()
        str_num_e1 = []
        for line in lines_e1:
            str_num_e1.append(line)
        for num in str_num_e1:
            data_e.append(float(num))
    
    with open('../Logs/Default_20210923_run2/5358_default-new-blueflood-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_f1:
        lines_f1 = file_f1.readlines()
        str_num_f1 = []
        for line in lines_f1:
            str_num_f1.append(line)
        for num in str_num_f1:
            data_f.append(float(num))
 
    with open('../Logs/Default_20210923_run2/5355_default-new-blueflood-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_g1:
        lines_g1 = file_g1.readlines()
        str_num_g1 = []
        for line in lines_g1:
            str_num_g1.append(line)
        for num in str_num_g1:
            data_g.append(float(num))

    with open('../Logs/Default_20210923_run2/5356_default-new-blueflood-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_h1:
        lines_h1 = file_h1.readlines()
        str_num_h1 = []
        for line in lines_h1:
            str_num_h1.append(line)
        for num in str_num_h1:
            data_h.append(float(num))

    with open('../Logs/Default_20210923/5317_default-new-blueflood-mode5/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_e2:
        lines_e2 = file_e2.readlines()
        str_num_e2 = []
        for line in lines_e2:
            str_num_e2.append(line)
        for num in str_num_e2:
            data_e.append(float(num))
    
    with open('../Logs/Default_20210923/5318_default-new-blueflood-mode6/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_f2:
        lines_f2 = file_f2.readlines()
        str_num_f2 = []
        for line in lines_f2:
            str_num_f2.append(line)
        for num in str_num_f2:
            data_f.append(float(num))

    with open('../Logs/Default_20210923/5315_default-new-blueflood-mode3/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_g2:
        lines_g2 = file_g2.readlines()
        str_num_g2 = []
        for line in lines_g2:
            str_num_g2.append(line)
        for num in str_num_g2:
            data_g.append(float(num))

    with open('../Logs/Default_20210923/5316_default-new-blueflood-mode4/logs/packet-delivery-rate.txt', encoding="iso-8859-14") as file_h2:
        lines_h2 = file_h2.readlines()
        str_num_h2 = []
        for line in lines_h2:
            str_num_h2.append(line)
        for num in str_num_h2:
            data_h.append(float(num))

    mean_e = np.mean(data_e)
    mean_f = np.mean(data_f)
    mean_g = np.mean(data_g)
    mean_h = np.mean(data_h)

    std_e = np.std(data_e)
    std_f = np.std(data_f)
    std_g = np.std(data_g)
    std_h = np.std(data_h)

    ###################################### The plot ###################################################
    plt.clf()

    fig, ax = plt.subplots()

    ind = np.arange(N)  # the 4 modes
    width = 0.35 

    ax.yaxis.grid(True)
    plt.ylabel("PDR in %")

    rects1 = ax.bar(ind, [mean_a,mean_b,mean_c,mean_d], width, yerr=[std_a,std_b,std_c,std_d])
    rects2 = ax.bar(ind+width, [mean_e,mean_f,mean_g,mean_h], width, yerr=[std_e,std_f,std_g,std_h])


    ax.set_xticks(ind + width / 2)
    ax.set_xticklabels( ('125Kbps', '500Kbps', '1Mbps', '2Mbps') )
    plt.axis([None, None, 0, 100])
    # Adapt the figure size as needed
    fig.set_size_inches(5.0, 8.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210921","pdr","all-runs","Bar", 5.0, 8.0)) + ".pdf", format="pdf")

    fig.set_size_inches(4.0, 4.0)
    plt.tight_layout()
    plt.savefig(export_dir + slugify(("20210921","pdr","all-runs","Bar", 4.0, 4.0)) + ".pdf", format="pdf")

    plt.close()

def export_example_3(config, export_dir):
    pass


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
