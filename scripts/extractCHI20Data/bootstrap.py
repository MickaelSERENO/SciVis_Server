import numpy as np

def bootstrap(data, nsample=1000, func=np.mean):
    """
    Generate `n` bootstrap samples, evaluating `func`
    at each resampling. `bootstrap` returns a function,
    which can be called to obtain confidence intervals
    of interest.

    This is the bootstrap percentile method found at http://www.jtrive.com/the-empirical-bootstrap-for-confidence-intervals-in-python.html

    @param data the data to bootstrap. Can be an array of 1 or 2 dimensions. If dimensions is equal to 2, we sample using the same index for all the rows
    @param nsample the number of iterations
    @param func the statistical function to apply
    @return a function permitting to compute confidence interface. Do val(0.95) to get 95% of confidence interval. This returns a tuple: (lower, upper)
    """
    simulations = list()
    sample_size = len(data)
    if len(data.shape) == 1:
        for c in range(nsample):
            #Take a new sample from this data with replacement.
            itersample = np.random.choice(data, size=sample_size, replace=True)
            #Compute the statistic and store it
            simulations.append(func(itersample))

    elif len(data.shape) == 2:
        for c in range(nsample):
            m,n = data.shape
            idx = np.random.randint(0,m,m)
            itersample = data[idx]
            value = func(itersample)
            simulations.append(value)

    #Sort our bootstrap array
    simulations.sort()
    def ci(p):
        """
        Return 2-sided symmetric confidence interval specified
        by p.
        """
        u_pval = (1+p)/2.
        l_pval = (1-u_pval)
        l_indx = int(np.floor(nsample*l_pval))
        u_indx = int(np.floor(nsample*u_pval))

        if u_indx == nsample:
            u_indx = nsample-1

        return(simulations[l_indx], simulations[u_indx])
    return(ci)

def getMeanAndStd(cis):
    """ Get the mean and the standard error of a confidence interval
    @param cis a tuple containing the confidence interval (lower, upper)
    @return (mean, std)"""

    return ((cis[0]+cis[1])/2, (cis[1]-cis[0])/2)

def getMeansAndStds(cis):
    """ Get the mean and the standard error of a multiple confidence intervals
    @param cis list of tuples containing the confidence interval (lower, upper)
    @return [(means), (stds)]"""

    return list(zip(*[getMeanAndStd(x) for x in cis]))
