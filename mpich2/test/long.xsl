<?xml version='1.0' ?>
<!-- <xsl:stylesheet  xmlns:xsl="http://www.w3.org/TR/WD-xsl"> -->
<xsl:stylesheet  xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<!-- <xsl:output method="html"/>-->
<xsl:template match="/">

<html>
<head>
<title>MPICH Error Report</title>
</head>
<body>
    <h1> Error Report </h1>
    <table cellpadding="1" border="1" bgcolor="white" cellspace="1">
    <xsl:apply-templates select="MPITESTRESULTS"/>
    </table>
</body>
</html>
</xsl:template>

<xsl:template match="MPITESTRESULTS">
    <xsl:apply-templates select="DATE"/>
    <xsl:apply-templates select="MPITEST"/>
</xsl:template>

<xsl:template match="DATE">
    <tr><td bgcolor="white" colspan="4">Test run on  <xsl:value-of select="."/></td></tr>
</xsl:template>

<xsl:template match="MPITEST">
    <tr bgcolor="white">
    <td valign="top">	
    <xsl:value-of select="NAME"/>
    </td><td valign="top">
    <xsl:if test="STATUS = 'pass'">
	<td><font color="green"><xsl:value-of select="STATUS"/></font></td>
    </xsl:if>
    <xsl:if test="STATUS = 'fail'">
	<td><font color="red"><xsl:value-of select="STATUS"/></font></td>
    </xsl:if>
    </td>
    <td WIDTH="40%"><pre>
    <xsl:value-of select="TESTDIFF"/>
    </pre>
    </td>
    <td valign="top">
    <pre>
    <xsl:value-of select="TRACEBACK"/>
    </pre>
    </td>
    </tr>
</xsl:template>

<xsl:template match="TRACEBACK">
    <a>
    <xsl:attribute name="HREF">
    <xsl:value-of select="."/>
    </xsl:attribute>
    Traceback
    </a>
</xsl:template>


</xsl:stylesheet>
