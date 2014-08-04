library(ggplot2)

#simple r code for scatter plot

# sample from poisson
ldat <- rpois(60,5)
rdat <- rpois(40,9)

type <- c(rep("A",60),rep("B",40))
y <- c(ldat,rdat)
x <- seq_along(type)

df <- data.frame(type,x,y)

# the scatter plot
# geom_point(shape=1...)
base <- ggplot(df,aes(x=x,y=y,color=type)) + geom_point(size=4)
dev.new(width=16,height=9)
print(base)

# save the picture 
dev.copy2eps()
