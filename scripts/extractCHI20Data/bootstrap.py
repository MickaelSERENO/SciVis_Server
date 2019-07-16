import numpy as np

def bootstrap(data, n=1000, func=np.mean):
    """
    Generate `n` bootstrap samples, evaluating `func`
    at each resampling. `bootstrap` returns a function,
    which can be called to obtain confidence intervals
    of interest.

    This is the bootstrap percentile method found at http://www.jtrive.com/the-empirical-bootstrap-for-confidence-intervals-in-python.html
    """
    simulations = list()
    sample_size = len(data)
    xbar_init = np.mean(data)
    for c in range(n):
        #Take a new sample from this data with replacement.
        itersample = np.random.choice(data, size=sample_size, replace=True)
        #Compute the statistic and store it
        simulations.append(func(itersample))

    #Sort our bootstrap array
    simulations.sort()
    def ci(p):
        """
        Return 2-sided symmetric confidence interval specified
        by p.
        """
        p = max(min(p, 1.0), 0.0) #Ensure p is between 0.0 and 1.0
        
        u_pval = (1+p)/2.
        l_pval = (1-u_pval)
        l_indx = int(np.floor(n*l_pval))
        u_indx = int(np.floor(n*u_pval))
        return(simulations[l_indx], simulations[u_indx])
    return(ci)
